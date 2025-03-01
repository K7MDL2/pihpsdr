/* Copyright (C)
* 2020 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <gtk/gtk.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

#include "main.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "band.h"
#include "receiver.h"
#include "transmitter.h"
#include "receiver.h"
#include "adc.h"
#include "dac.h"
#include "radio.h"
#include "actions.h"
#include "action_dialog.h"
#include "midi.h"
#include "alsa_midi.h"
#include "new_menu.h"
#include "midi_menu.h"
#include "property.h"

enum {
  EVENT_COLUMN,
  CHANNEL_COLUMN,
  NOTE_COLUMN,
  TYPE_COLUMN,
  ACTION_COLUMN,
  N_COLUMNS
};

static GtkWidget *dialog=NULL;

static GtkListStore *store;
static GtkWidget *view;
static GtkWidget *scrolled_window=NULL;
static gulong selection_signal_id;
static GtkTreeModel *model;
static GtkTreeIter iter;
struct desc *current_cmd;

static GtkWidget *newEvent;
static GtkWidget *newChannel;
static GtkWidget *newNote;
static GtkWidget *newVal;
static GtkWidget *newType;
static GtkWidget *newMin;
static GtkWidget *newMax;
static GtkWidget *newAction;
static GtkWidget *configure_b;
static GtkWidget *any_b;
static GtkWidget *add_b;
static GtkWidget *update_b;
static GtkWidget *delete_b;
static GtkWidget *device_b[MAX_MIDI_DEVICES];

static enum MIDIevent thisEvent=EVENT_NONE;
static int thisChannel;
static int thisNote;
static int thisVal;
static int thisMin;
static int thisMax;
static int thisDelay;
static int thisVfl1, thisVfl2;
static int thisFl1,  thisFl2;
static int thisLft1, thisLft2;
static int thisRgt1, thisRgt2;
static int thisFr1,  thisFr2;
static int thisVfr1, thisVfr2;

static GtkWidget *WheelContainer;
static GtkWidget *set_delay;
static GtkWidget *set_vfl1, *set_vfl2;
static GtkWidget *set_fl1,  *set_fl2;
static GtkWidget *set_lft1, *set_lft2;
static GtkWidget *set_rgt1, *set_rgt2;
static GtkWidget *set_fr1,  *set_fr2;
static GtkWidget *set_vfr1, *set_vfr2;

static enum ACTIONtype thisType;
static int thisAction;

static gboolean accept_any=FALSE;

enum {
  UPDATE_NEW,
  UPDATE_CURRENT,
  UPDATE_EXISTING
};

static int update(void *data);
static void load_store();

static void cleanup() {
  configure_midi_device(FALSE);
  if(dialog!=NULL) {
    gtk_widget_destroy(dialog);
    dialog=NULL;
    sub_menu=NULL;
  }
}

static gboolean close_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  cleanup();
  return TRUE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  cleanup();
  return FALSE;
}

static void device_cb(GtkWidget *widget, gpointer data) {
  int index=GPOINTER_TO_INT(data);
  int val=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  if (val == 1) {
    register_midi_device(index);
  } else {
    close_midi_device(index);
  }
  // take care button remains un-checked if opening failed
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), midi_devices[index].active);
}

static void configure_cb(GtkWidget *widget, gpointer data) {
  gboolean conf=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (widget));
  configure_midi_device(conf);
  if (conf) {
    gtk_widget_show(any_b);
  } else {
    gtk_widget_hide(any_b);
  }
}

static void any_cb(GtkWidget *widget, gpointer data) {
  gboolean conf=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (widget));
  accept_any = conf;
}

static void update_wheelparams(gpointer user_data) {
  //
  // Task: show or hide WheelContainer depending on whether
  //       thre current type is a wheel. If it is a wheel,
  //       set spin buttons to current values.
  //
  if (thisType==MIDI_WHEEL) {
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_delay),(double) thisDelay);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_vfl1 ),(double) thisVfl1 );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_vfl2 ),(double) thisVfl2 );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_fl1  ),(double) thisFl1  );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_fl2  ),(double) thisFl2  );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_lft1 ),(double) thisLft1 );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_lft2 ),(double) thisLft2 );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_rgt1 ),(double) thisRgt1 );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_rgt2 ),(double) thisRgt2 );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_fr1  ),(double) thisFr1  );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_fr2  ),(double) thisFr2  );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_vfr1 ),(double) thisVfr1 );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(set_vfr2 ),(double) thisVfr2 );
    gtk_widget_show(WheelContainer);
  } else {
    gtk_widget_hide(WheelContainer);
  }
}

static void type_changed_cb(GtkWidget *widget, gpointer data) {

  // update actions available for the type
  gchar *type=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));

  if (type == NULL) {
    //
    // This happens if the combo-box is cleared in update()
    //
    return;
  }
  //g_print("%s: type=%s action=%d type=%d\n",__FUNCTION__,type,thisAction,thisType);

  if(strcmp(type,"KEY")==0) {
    thisType=MIDI_KEY;
  } else if(strcmp(type,"KNOB/SLIDER")==0) {
    thisType=MIDI_KNOB;
  } else if(strcmp(type,"WHEEL")==0) {
    thisType=MIDI_WHEEL;
  } else {
    thisType=TYPE_NONE;
  }
  //
  // If the type changed, the current action may no longer be allowed
  //
  if (ActionTable[thisAction].type && thisType == 0) {
    thisAction=NO_ACTION;
  }

  gtk_button_set_label(GTK_BUTTON(newAction),ActionTable[thisAction].str);
  update_wheelparams(NULL);
}

static gboolean action_cb(GtkWidget *widget,gpointer data) {
  int selection=0;
  gchar *type=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(newType));
  if(type==NULL || strcmp(type,"NONE")==0) {
    return TRUE;
  } else if(strcmp(type,"KEY")==0) {
    selection=MIDI_KEY;
  } else if(strcmp(type,"KNOB/SLIDER")==0) {
    selection=MIDI_KNOB;
  } else if(strcmp(type,"WHEEL")==0) {
    selection=MIDI_WHEEL | MIDI_KNOB;
  }
g_print("%s: type=%s selection=%02X thisAction=%d\n",__FUNCTION__,type,selection,thisAction);
  int action=action_dialog(dialog,selection,thisAction);
  thisAction=action;
  gtk_button_set_label(GTK_BUTTON(newAction),ActionTable[action].str);
  return TRUE;
}

static void row_inserted_cb(GtkTreeModel *tree_model,GtkTreePath *path, GtkTreeIter *iter,gpointer user_data) {
  //g_print("%s\n",__FUNCTION__);
  gtk_tree_view_set_cursor(GTK_TREE_VIEW(view),path,NULL,FALSE);
}


static void tree_selection_changed_cb (GtkTreeSelection *selection, gpointer data) {
  char *str_event;
  char *str_channel;
  char *str_note;
  char *str_type;
  char *str_action;
  int i;

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get(model, &iter, EVENT_COLUMN, &str_event, -1);
    gtk_tree_model_get(model, &iter, CHANNEL_COLUMN, &str_channel, -1);
    gtk_tree_model_get(model, &iter, NOTE_COLUMN, &str_note, -1);
    gtk_tree_model_get(model, &iter, TYPE_COLUMN, &str_type, -1);
    gtk_tree_model_get(model, &iter, ACTION_COLUMN, &str_action, -1);

    if(str_event!=NULL && str_channel!=NULL && str_note!=NULL && str_type!=NULL && str_action!=NULL) {

      if(strcmp(str_event,"CTRL")==0) {
        thisEvent=MIDI_CTRL;
      } else if(strcmp(str_event,"PITCH")==0) {
        thisEvent=MIDI_PITCH;
      } else if(strcmp(str_event,"NOTE")==0) {
        thisEvent=MIDI_NOTE;
      } else {
        thisEvent=EVENT_NONE;
      }
      if (!strncmp(str_channel,"Any",3)) {
        thisChannel=-1;
      } else {
        thisChannel=atoi(str_channel);
      }
      thisNote=atoi(str_note);
      thisVal=0;
      thisMin=0;
      thisMax=0;
      if(strcmp(str_type,"KEY")==0) {
        thisType=MIDI_KEY;
      } else if(strcmp(str_type,"KNOB/SLIDER")==0) {
        thisType=MIDI_KNOB;
      } else if(strcmp(str_type,"WHEEL")==0) {
        thisType=MIDI_WHEEL;
      } else {
        thisType=TYPE_NONE;
      }

      thisAction=NO_ACTION;
      for(i=0;i<ACTIONS;i++) {
        if(strcmp(ActionTable[i].str,str_action)==0 && (thisType & ActionTable[i].type)) {
          thisAction=ActionTable[i].action;
          break;
        }
      }
      g_idle_add(update,GINT_TO_POINTER(UPDATE_EXISTING));
    }
  }
}

static void find_current_cmd() {
  struct desc *cmd;
  cmd=MidiCommandsTable[thisNote];
  while(cmd!=NULL) {
    if((cmd->channel==thisChannel || cmd->channel==-1) && cmd->type==thisType && cmd->action==thisAction) {
      break;
    }
    cmd=cmd->next;
  }
  current_cmd=cmd;
}

static void wheelparam_cb(GtkWidget *widget, gpointer user_data) {
  int what = GPOINTER_TO_INT(user_data);
  int val=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  int newval=val;

  if (thisType != MIDI_WHEEL) {
    // we should never arrive here
    return;
  }
  switch (what) {
      case 1:  // Delay
        thisDelay=newval;
        break;
     case 2:  // Very fast Left 1
        if (newval > thisVfl2) newval=thisVfl2;
        thisVfl1=newval;
        break;
     case 3:  // Very fast Left 2
        if (newval < thisVfl1) newval=thisVfl1;
        thisVfl2=newval;
        break;
     case 4:  // Fast Left 1
        if (newval > thisFl2) newval=thisFl2;
        thisFl1=newval;
        break;
     case 5:  // Fast Left 2
        if (newval < thisFl1) newval=thisFl1;
        thisFl2=newval;
        break;
     case 6:  // Left 1
        if (newval > thisLft2) newval=thisLft2;
        thisLft1=newval;
        break;
     case 7:  // Left 2
        if (newval < thisLft1) newval=thisLft1;
        thisLft2=newval;
        break;
     case 8:  // Right 1
        if (newval > thisRgt2) newval=thisRgt2;
        thisRgt1=newval;
        break;
     case 9:  // Right 2
        if (newval < thisRgt1) newval=thisRgt1;
        thisRgt2=newval;
        break;
     case 10:  // Fast Right 1
        if (newval > thisFr2) newval=thisFr2;
        thisFr1=newval;
        break;
     case 11:  // Fast Right2 
        if (newval < thisFr1) newval=thisFr1;
        thisFr2=newval;
        break;
     case 12:  // Very fast Right 1
        if (newval > thisVfr2) newval=thisVfr2;
        thisVfr1=newval;
        break;
     case 13:  // Very fast Right 2
        if (newval < thisVfr1) newval=thisVfr1;
        thisVfr2=newval;
        break;
  }
  //
  // If we have changed the value because we kept thisVfl2 >= thisVfl1 etc,
  // update the spin button
  //
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),(double) newval);
}

static void clear_cb(GtkWidget *widget,gpointer user_data) {

  gtk_list_store_clear(store);
  MidiReleaseCommands();  
}

static void save_cb(GtkWidget *widget,gpointer user_data) {
  GtkWidget *save_dialog;
  GtkFileChooser *chooser;
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
  gint res;

  save_dialog = gtk_file_chooser_dialog_new ("Save File",
                                      GTK_WINDOW(dialog),
                                      action,
                                      "_Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "_Save",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);
  chooser = GTK_FILE_CHOOSER (save_dialog);
  gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);
  gtk_file_chooser_set_current_name(chooser,"midi.midi");
  res = gtk_dialog_run (GTK_DIALOG (save_dialog));
  if(res==GTK_RESPONSE_ACCEPT) {
    char *savefilename=gtk_file_chooser_get_filename(chooser);
    clearProperties();
    midi_save_state();
    saveProperties(savefilename);
    g_free(savefilename);
  }
  gtk_widget_destroy(save_dialog);
}

static void load_cb(GtkWidget *widget,gpointer user_data) {
  GtkWidget *load_dialog;
  GtkFileChooser *chooser;
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
  gint res;

  load_dialog = gtk_file_chooser_dialog_new ("Open MIDI File",
                                      GTK_WINDOW(dialog),
                                      action,
                                      "_Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "_Load",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);
  chooser = GTK_FILE_CHOOSER (load_dialog);
  res = gtk_dialog_run (GTK_DIALOG (load_dialog));
  if(res==GTK_RESPONSE_ACCEPT) {
    char *loadfilename=gtk_file_chooser_get_filename(chooser);
    clear_cb(NULL,NULL);
    clearProperties();
    loadProperties(loadfilename);
    midi_restore_state();
    load_store();
    g_free(loadfilename);
  }
  gtk_widget_destroy(load_dialog);
}

static void load_original_cb(GtkWidget *widget,gpointer user_data) {
  GtkWidget *load_dialog;
  GtkFileChooser *chooser;
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
  gint res;

  load_dialog = gtk_file_chooser_dialog_new ("Open ORIGINAL MIDI File",
                                      GTK_WINDOW(dialog),
                                      action,
                                      "_Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "_Load",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);
  chooser = GTK_FILE_CHOOSER (load_dialog);
  res = gtk_dialog_run (GTK_DIALOG (load_dialog));
  if(res==GTK_RESPONSE_ACCEPT) {
    char *loadfilename=gtk_file_chooser_get_filename(chooser);
    clear_cb(NULL,NULL);
    ReadLegacyMidiFile(loadfilename);
    load_store();
    g_free(loadfilename);
  }
  gtk_widget_destroy(load_dialog);
}

static void add_store(int key,struct desc *cmd) {
  char str_event[16];
  char str_channel[16];
  char str_note[16];
  char str_type[32];
  char str_action[32];

  //g_print("%s: key=%d desc=%p\n",__FUNCTION__,key,cmd);
  switch(cmd->event) {
    case EVENT_NONE:
      strcpy(str_event,"NONE");
      break;
    case MIDI_NOTE:
      strcpy(str_event,"NOTE");
      break;
    case MIDI_CTRL:
      strcpy(str_event,"CTRL");
      break;
    case MIDI_PITCH:
      strcpy(str_event,"PITCH");
      break;
  }
  if (cmd->channel >= 0) {
    sprintf(str_channel,"%d",cmd->channel);
  } else {
    sprintf(str_channel,"%s","Any");
  }
  sprintf(str_note,"%d",key);
  switch(cmd->type) {
    case TYPE_NONE:
      strcpy(str_type,"NONE");
      break;
    case MIDI_KEY:
      strcpy(str_type,"KEY");
      break;
    case MIDI_KNOB:
      strcpy(str_type,"KNOB/SLIDER");
      break;
    case MIDI_WHEEL:
      strcpy(str_type,"WHEEL");
      break;
    default:
      // other types cannot occur for MIDI
      break;
  }
  strcpy(str_action,ActionTable[cmd->action].str);
  gtk_list_store_prepend(store,&iter);
  gtk_list_store_set(store,&iter,
      EVENT_COLUMN,str_event,
      CHANNEL_COLUMN,str_channel,
      NOTE_COLUMN,str_note,
      TYPE_COLUMN,str_type,
      ACTION_COLUMN,str_action,
      -1);

  if(scrolled_window!=NULL) {
    GtkAdjustment *adjustment=gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(scrolled_window));
    //g_print("%s: adjustment=%f lower=%f upper=%f\n",__FUNCTION__,gtk_adjustment_get_value(adjustment),gtk_adjustment_get_lower(adjustment),gtk_adjustment_get_upper(adjustment));
    if(gtk_adjustment_get_value(adjustment)!=0.0) {
      gtk_adjustment_set_value(adjustment,0.0);
    }
  }
}

static void load_store() {
  struct desc *cmd;
  gtk_list_store_clear(store);
  for(int i=127;i>=0;i--) {
    cmd=MidiCommandsTable[i];
    while(cmd!=NULL) {
      add_store(i,cmd);
      cmd=cmd->next;
    }
  }
}

static void add_cb(GtkButton *widget,gpointer user_data) {

  gchar *str_type=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(newType));
  //gchar *str_action=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(newAction));
  const gchar *str_action=gtk_button_get_label(GTK_BUTTON(newAction));
;

  gint i;
  gint type;
  gint action;

  if(str_type==NULL || str_action==NULL) {
    return;
  }

  if(strcmp(str_type,"KEY")==0) {
    type=MIDI_KEY;
  } else if(strcmp(str_type,"KNOB/SLIDER")==0) {
    type=MIDI_KNOB;
  } else if(strcmp(str_type,"WHEEL")==0) {
    type=MIDI_WHEEL;
  } else {
    type=TYPE_NONE;
  }

  action=NO_ACTION;
  for(i=0;i<ACTIONS;i++) {
    if(strcmp(ActionTable[i].str,str_action)==0 && (type & ActionTable[i].type)) {
      action=ActionTable[i].action;
      break;
    }
  }

  g_print("%s: type=%s (%d) action=%s (%d)\n",__FUNCTION__,str_type,type,str_action,action);

  struct desc *desc;
  desc = (struct desc *) malloc(sizeof(struct desc));
  desc->next = NULL;
  desc->action = action; // MIDIaction
  desc->type = type; // MIDItype
  desc->event = thisEvent; // MIDevent
  desc->delay = thisDelay;
  desc->vfl1  = thisVfl1;
  desc->vfl2  = thisVfl2;
  desc->fl1   = thisFl1;
  desc->fl2   = thisFl2;
  desc->lft1  = thisLft1;
  desc->lft2  = thisLft2;
  desc->rgt1  = thisRgt1;
  desc->rgt2  = thisRgt2;
  desc->fr1   = thisFr1;
  desc->fr2   = thisFr2;
  desc->vfr1  = thisVfr1;
  desc->vfr2  = thisVfr2;
  desc->channel  = thisChannel;

  gint key=thisNote;
  if(key<0) key=0;
  if(key>127) key=0;


  MidiAddCommand(key, desc);
  add_store(key,desc);
  current_cmd=desc;

  gtk_widget_set_sensitive(add_b,FALSE);
  gtk_widget_set_sensitive(update_b,TRUE);
  gtk_widget_set_sensitive(delete_b,TRUE);

}

static void update_cb(GtkButton *widget,gpointer user_data) {
  char str_event[16];
  char str_channel[16];
  char str_note[16];
  int i;


  gchar *str_type=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(newType));
  //gchar *str_action=gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(newAction));
  const gchar *str_action=gtk_button_get_label(GTK_BUTTON(newAction));
;
  //g_print("%s: type=%s action=%s\n",__FUNCTION__,str_type,str_action);

  if (current_cmd == NULL) {
    g_print("%s: current_cmd is NULL!\n", __FUNCTION__);
    return;
  }

  if(strcmp(str_type,"KEY")==0) {
    thisType=MIDI_KEY;
  } else if(strcmp(str_type,"KNOB/SLIDER")==0) {
    thisType=MIDI_KNOB;
  } else if(strcmp(str_type,"WHEEL")==0) {
    thisType=MIDI_WHEEL;
  } else {
    thisType=TYPE_NONE;
  }

  thisAction=NO_ACTION;
  for(i=0;i<ACTIONS;i++) {
    if(strcmp(ActionTable[i].str,str_action)==0 && (thisType & ActionTable[i].type)) {
      thisAction=ActionTable[i].action;
      break;
    }
  }

  current_cmd->channel=thisChannel;
  current_cmd->type   =thisType;
  current_cmd->action =thisAction;
  current_cmd->delay =thisDelay;

  current_cmd->vfl1  =thisVfl1;
  current_cmd->vfl2  =thisVfl2;
  current_cmd->fl1   =thisFl1;
  current_cmd->fl2   =thisFl2;
  current_cmd->lft1  =thisLft1;
  current_cmd->lft2  =thisLft2;
  current_cmd->rgt1  =thisRgt1;
  current_cmd->rgt2  =thisRgt2;
  current_cmd->fr1   =thisFr1;
  current_cmd->fr2   =thisFr2;
  current_cmd->vfr1  =thisVfr1;
  current_cmd->vfr2  =thisVfr2;

  if (accept_any) current_cmd->channel=-1;

  switch(current_cmd->event) {
    case EVENT_NONE:
      strcpy(str_event,"NONE");
      break;
    case MIDI_NOTE:
      strcpy(str_event,"NOTE");
      break;
    case MIDI_CTRL:
      strcpy(str_event,"CTRL");
      break;
    case MIDI_PITCH:
      strcpy(str_event,"PITCH");
      break;
  }
  if (current_cmd->channel >= 0) {
    sprintf(str_channel,"%d",current_cmd->channel);
  } else {
    sprintf(str_channel,"%s","Any");
  }
  sprintf(str_note,"%d",thisNote);

  //g_print("%s: event=%s channel=%s note=%s type=%s action=%s\n",
  //        __FUNCTION__,str_event,str_channel,str_note,str_type,str_action);
  gtk_list_store_set(store,&iter,
      EVENT_COLUMN,str_event,
      CHANNEL_COLUMN,str_channel,
      NOTE_COLUMN,str_note,
      TYPE_COLUMN,str_type,
      ACTION_COLUMN,str_action,
      -1);
}

static void delete_cb(GtkButton *widget,gpointer user_data) {
  struct desc *previous_cmd;
  struct desc *next_cmd;
  GtkTreeIter saved_iter;
  g_print("%s: thisNote=%d current_cmd=%p\n",__FUNCTION__,thisNote,current_cmd);

  if (current_cmd == NULL) {
    g_print("%s: current_cmd is NULL!\n", __FUNCTION__);
    return;
  }

  saved_iter=iter;


  // remove from MidiCommandsTable
  if(MidiCommandsTable[thisNote]==current_cmd) {
    g_print("%s: remove first\n",__FUNCTION__);
    MidiCommandsTable[thisNote]=current_cmd->next;
    g_free(current_cmd);
    current_cmd=NULL;
  } else {
    previous_cmd=MidiCommandsTable[thisNote];
    while(previous_cmd->next!=NULL) {
      next_cmd=previous_cmd->next;
      if(next_cmd==current_cmd) {
        g_print("%s: remove next\n",__FUNCTION__);
	previous_cmd->next=next_cmd->next;
	g_free(next_cmd);
        current_cmd=NULL;  // note next_cmd == current_cmd
	break;
      }
      previous_cmd=next_cmd;
    }
  }

  // remove from list store
  gtk_list_store_remove(store,&saved_iter);

  gtk_widget_set_sensitive(add_b,TRUE);
  gtk_widget_set_sensitive(update_b,FALSE);
  gtk_widget_set_sensitive(delete_b,FALSE);

}

void midi_menu(GtkWidget *parent) {
  int i;
  int col;
  int row;
  GtkCellRenderer *renderer;
  GtkWidget *lbl;

  dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(parent));
  char title[64];
  sprintf(title,"piHPSDR - MIDI");
  gtk_window_set_title(GTK_WINDOW(dialog),title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (delete_event), NULL);

  GdkRGBA color;
  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;
  gtk_widget_override_background_color(dialog,GTK_STATE_FLAG_NORMAL,&color);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(grid),2);

  row=0;
  col=0;

  GtkWidget *close_b=gtk_button_new_with_label("Close");
  g_signal_connect(close_b, "pressed", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), close_b, col, row, 1, 1);
  col++;

  get_midi_devices();
  if (n_midi_devices > 0) {
    GtkWidget *devices_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(devices_label), "<b>Select MIDI device(s)</b>");
    gtk_label_set_justify(GTK_LABEL(devices_label),GTK_JUSTIFY_LEFT);
    gtk_grid_attach(GTK_GRID(grid),devices_label,col,row,2,1);
    //
    // Now put the device checkboxes in columns 3 (width: 1), 4 (width: 3), 7 (width: 1)
    // and make as many rows as necessary
    col=3;
    int width = 1;
    for (i=0; i<n_midi_devices; i++) {
      device_b[i] = gtk_check_button_new_with_label(midi_devices[i].name);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(device_b[i]), midi_devices[i].active);
      gtk_grid_attach(GTK_GRID(grid),device_b[i],col,row,width,1);
      switch (col) {
	case 3:
	  col=4;
          width=3;
	  break;
        case 4:
	  col=7;
          width=1;
          break;
	case 7:
	  col=3;
          width=1;
          row++;
	  break;
      }
      g_signal_connect(device_b[i], "toggled", G_CALLBACK(device_cb), GINT_TO_POINTER(i));
      gtk_widget_show(device_b[i]);
    }
    //
    // Row containing device checkboxes is partially filled,
    // advance to next one.
    if (col > 3) {
      col=0;
      row++;
    }
  } else {
    GtkWidget *devices_label=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(devices_label), "<b>No MIDI devices found!</b>");
    gtk_label_set_justify(GTK_LABEL(devices_label),GTK_JUSTIFY_LEFT);
    gtk_grid_attach(GTK_GRID(grid),devices_label,col,row,3,1);
    row++;
    col=0;
  }

  row++;
  col=0;

  GtkWidget *clear_b=gtk_button_new_with_label("Clear");
  gtk_grid_attach(GTK_GRID(grid),clear_b,col,row,1,1);
  g_signal_connect(clear_b,"clicked",G_CALLBACK(clear_cb),NULL);
  col++;

  GtkWidget *save_b=gtk_button_new_with_label("Save");
  gtk_grid_attach(GTK_GRID(grid),save_b,col,row,1,1);
  g_signal_connect(save_b,"clicked",G_CALLBACK(save_cb),NULL);
  col++;

  GtkWidget *load_b=gtk_button_new_with_label("Load");
  gtk_grid_attach(GTK_GRID(grid),load_b,col,row,1,1);
  g_signal_connect(load_b,"clicked",G_CALLBACK(load_cb),NULL);
  col++;

  GtkWidget *load_original_b=gtk_button_new_with_label("Load Original");
  gtk_grid_attach(GTK_GRID(grid),load_original_b,col,row,1,1);
  g_signal_connect(load_original_b,"clicked",G_CALLBACK(load_original_cb),NULL);
  col++;

  configure_b=gtk_check_button_new_with_label("MIDI Configure");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (configure_b), FALSE);
  gtk_grid_attach(GTK_GRID(grid),configure_b,col,row,3,1);
  g_signal_connect(configure_b,"toggled",G_CALLBACK(configure_cb),NULL);

  col+=3;
  any_b=gtk_check_button_new_with_label("Configure for any channel");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (any_b), FALSE);
  gtk_grid_attach(GTK_GRID(grid),any_b,col,row,6,1);
  g_signal_connect(any_b,"toggled",G_CALLBACK(any_cb),NULL);

  row++;
  col=0;
  GtkWidget *label=gtk_label_new("Evt");
  gtk_grid_attach(GTK_GRID(grid),label,col++,row,1,1);
  label=gtk_label_new("Ch");
  gtk_grid_attach(GTK_GRID(grid),label,col++,row,1,1);
  label=gtk_label_new("Note");
  gtk_grid_attach(GTK_GRID(grid),label,col++,row,1,1);
  label=gtk_label_new("Type");
  gtk_grid_attach(GTK_GRID(grid),label,col++,row,1,1);
  label=gtk_label_new("Value");
  gtk_grid_attach(GTK_GRID(grid),label,col++,row,1,1);
  label=gtk_label_new("Min");
  gtk_grid_attach(GTK_GRID(grid),label,col++,row,1,1);
  label=gtk_label_new("Max");
  gtk_grid_attach(GTK_GRID(grid),label,col++,row,1,1);
  label=gtk_label_new("Action");
  gtk_grid_attach(GTK_GRID(grid),label,col++,row,1,1);


  row++;
  col=0;
  newEvent=gtk_label_new("");
  gtk_grid_attach(GTK_GRID(grid),newEvent,col++,row,1,1);
  newChannel=gtk_label_new("");
  gtk_grid_attach(GTK_GRID(grid),newChannel,col++,row,1,1);
  newNote=gtk_label_new("");
  gtk_grid_attach(GTK_GRID(grid),newNote,col++,row,1,1);
  newType=gtk_combo_box_text_new();
  gtk_grid_attach(GTK_GRID(grid),newType,col++,row,1,1);
  g_signal_connect(newType,"changed",G_CALLBACK(type_changed_cb),NULL);
  newVal=gtk_label_new("");
  gtk_grid_attach(GTK_GRID(grid),newVal,col++,row,1,1);
  newMin=gtk_label_new("");
  gtk_grid_attach(GTK_GRID(grid),newMin,col++,row,1,1);
  newMax=gtk_label_new("");
  gtk_grid_attach(GTK_GRID(grid),newMax,col++,row,1,1);

  newAction=gtk_button_new_with_label("    ");
  g_signal_connect(newAction, "button-press-event", G_CALLBACK(action_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),newAction,col,row,3,1);

  row++;
  col=0;

  add_b=gtk_button_new_with_label("Add");
  g_signal_connect(add_b, "pressed", G_CALLBACK(add_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),add_b,col++,row,1,1);
  gtk_widget_set_sensitive(add_b,FALSE);

  update_b=gtk_button_new_with_label("Update");
  g_signal_connect(update_b, "pressed", G_CALLBACK(update_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),update_b,col++,row,1,1);
  gtk_widget_set_sensitive(update_b,FALSE);

  delete_b=gtk_button_new_with_label("Delete");
  g_signal_connect(delete_b, "pressed", G_CALLBACK(delete_cb),NULL);
  gtk_grid_attach(GTK_GRID(grid),delete_b,col++,row,1,1);
  gtk_widget_set_sensitive(delete_b,FALSE);

  row++;
  col=0;

  scrolled_window=gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);
  gtk_widget_set_size_request(scrolled_window,400,200);

  view=gtk_tree_view_new();

  renderer=gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, "Event", renderer, "text", EVENT_COLUMN, NULL);

  renderer=gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, "CHANNEL", renderer, "text", CHANNEL_COLUMN, NULL);

  renderer=gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, "NOTE", renderer, "text", NOTE_COLUMN, NULL);

  renderer=gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, "TYPE", renderer, "text", TYPE_COLUMN, NULL);

  renderer=gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, "ACTION", renderer, "text", ACTION_COLUMN, NULL);

  store=gtk_list_store_new(N_COLUMNS,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING);

  load_store();

  gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));

  gtk_container_add(GTK_CONTAINER(scrolled_window),view);

  gtk_grid_attach(GTK_GRID(grid), scrolled_window, col, row, 5, 10);

  model=gtk_tree_view_get_model(GTK_TREE_VIEW(view));
  g_signal_connect(model,"row-inserted",G_CALLBACK(row_inserted_cb),NULL);

  GtkTreeSelection *selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

  selection_signal_id=g_signal_connect(G_OBJECT(selection),"changed",G_CALLBACK(tree_selection_changed_cb),NULL);

  //
  // Place a fixed container to hold the wheel parameters
  // and create sub-grid
  //
  col=5;
  WheelContainer=gtk_fixed_new();
  gtk_widget_set_size_request(WheelContainer,300,300-15*((n_midi_devices+1)/3));
  gtk_grid_attach(GTK_GRID(grid), WheelContainer, col, row, 6, 10);
  //
  // Showing/hiding the container may resize-the columns of the main grid,
  // and causing other elements to move around. Therefore create a further
  // "dummy" frame that is always shown. The dummy must have the same width
  // and a small height.
  //
  GtkWidget *DummyContainer=gtk_fixed_new();
  gtk_widget_set_size_request(DummyContainer,300,1);
  gtk_grid_attach(GTK_GRID(grid), DummyContainer, col, row, 6, 1);

  GtkWidget *WheelGrid=gtk_grid_new();
  gtk_grid_set_column_spacing (GTK_GRID(WheelGrid),2);

  col=0;
  row=0;

  lbl=gtk_label_new(NULL);
  // the new-line in the label get some space between the text and the spin buttons
  gtk_label_set_markup(GTK_LABEL(lbl), "<b>Configure special WHEEL parameters\n</b>");
  gtk_widget_set_size_request(lbl,300,30);
  gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);

  gtk_grid_attach(GTK_GRID(WheelGrid), lbl, col, row, 3, 1);

  //
  // Finally, put wheel config elements into the wheel grid
  //
  col=0;
  row++;

  lbl=gtk_label_new("Delay");
  gtk_widget_set_halign(lbl, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(WheelGrid), lbl, col, row, 1, 1);
  col++;

  set_delay = gtk_spin_button_new_with_range(0.0, 500.0, 10.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_delay, col, row, 1, 1);
  g_signal_connect(set_delay, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(1));
  col++;

  row++;
  col=0;
  lbl=gtk_label_new("Left <<<");
  gtk_widget_set_halign(lbl, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(WheelGrid), lbl, col, row, 1, 1);
  col++;

  set_vfl1 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_vfl1, col, row, 1, 1);
  g_signal_connect(set_vfl1, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(2));
  col++;

  set_vfl2 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_vfl2, col, row, 1, 1);
  g_signal_connect(set_vfl2, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(3));
  col++;

  row++;
  col=0;

  lbl=gtk_label_new("Left <<");
  gtk_widget_set_halign(lbl, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(WheelGrid), lbl, col, row, 1, 1);
  col++;
 
  set_fl1 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_fl1, col, row, 1, 1);
  g_signal_connect(set_fl1, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(4));
  col++;

  set_fl2 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_fl2, col, row, 1, 1);
  g_signal_connect(set_fl2, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(5));
  col++;

  row++;
  col=0;
  lbl=gtk_label_new("Left <");
  gtk_widget_set_halign(lbl, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(WheelGrid), lbl, col, row, 1, 1);
  col++;

  set_lft1 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_lft1, col, row, 1, 1);
  g_signal_connect(set_lft1, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(6));
  col++;

  set_lft2 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_lft2, col, row, 1, 1);
  g_signal_connect(set_lft2, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(7));
  col++;

  row++;
  col=0;
  lbl=gtk_label_new("Right >");
  gtk_widget_set_halign(lbl, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(WheelGrid), lbl, col, row, 1, 1);
  col++;

  set_rgt1 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_rgt1, col, row, 1, 1);
  g_signal_connect(set_rgt1, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(8));
  col++;

  set_rgt2 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_rgt2, col, row, 1, 1);
  g_signal_connect(set_rgt2, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(9));
  col++;

  row++;
  col=0;
  lbl=gtk_label_new("Right >>");
  gtk_widget_set_halign(lbl, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(WheelGrid), lbl, col, row, 1, 1);
  col++;

  set_fr1 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_fr1, col, row, 1, 1);
  g_signal_connect(set_fr1, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(10));
  col++;

  set_fr2 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_fr2, col, row, 1, 1);
  g_signal_connect(set_fr2, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(11));
  col++;

  row++;
  col=0;
  lbl=gtk_label_new("Right >>>");
  gtk_widget_set_halign(lbl, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(WheelGrid), lbl, col, row, 1, 1);
  col++;

  set_vfr1 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_vfr1, col, row, 1, 1);
  g_signal_connect(set_vfr1, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(12));
  col++;

  set_vfr2 = gtk_spin_button_new_with_range(-1.0, 127.0, 1.0);
  gtk_grid_attach(GTK_GRID(WheelGrid), set_vfr2, col, row, 1, 1);
  g_signal_connect(set_vfr2, "value-changed", G_CALLBACK(wheelparam_cb), GINT_TO_POINTER(13));
  col++;

  gtk_container_add(GTK_CONTAINER(content),grid);
  gtk_container_add(GTK_CONTAINER(WheelContainer), WheelGrid);
  sub_menu=dialog;
  gtk_widget_show_all(dialog);

  //
  // Hide "accept from any source" checkbox
  // (made visible only if config is checked)
  gtk_widget_hide(any_b);
  gtk_widget_hide(WheelContainer);
}

static int update(void *data) {
  int state=GPOINTER_TO_INT(data);
  gchar text[32];
  gint i=1;

  switch(state) {
    case UPDATE_NEW:
      switch(thisEvent) {
        case EVENT_NONE:
          gtk_label_set_text(GTK_LABEL(newEvent),"NONE");
          break;
        case MIDI_NOTE:
          gtk_label_set_text(GTK_LABEL(newEvent),"NOTE");
          break;
        case MIDI_CTRL:
          gtk_label_set_text(GTK_LABEL(newEvent),"CTRL");
          break;
        case MIDI_PITCH:
          gtk_label_set_text(GTK_LABEL(newEvent),"PITCH");
          break;
      }
      if (thisChannel >= 0) {
        sprintf(text,"%d",thisChannel);
      } else {
        strcpy(text,"Any");
      }
      gtk_label_set_text(GTK_LABEL(newChannel),text);
      sprintf(text,"%d",thisNote);
      gtk_label_set_text(GTK_LABEL(newNote),text);
      gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(newType));
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(newType),NULL,"NONE");
      switch(thisEvent) {
        case EVENT_NONE:
          gtk_combo_box_set_active (GTK_COMBO_BOX(newType),0);
          break;
        case MIDI_NOTE:
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(newType),NULL,"KEY");
          gtk_combo_box_set_active (GTK_COMBO_BOX(newType),1);
          break;
        case MIDI_CTRL:
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(newType),NULL,"KNOB/SLIDER");
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(newType),NULL,"WHEEL");
          gtk_combo_box_set_active (GTK_COMBO_BOX(newType),0);
          break;
        case MIDI_PITCH:
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(newType),NULL,"KNOB/SLIDER");
          gtk_combo_box_set_active (GTK_COMBO_BOX(newType),1);
      }
      gtk_button_set_label(GTK_BUTTON(newAction),ActionTable[0].str); // NONE
      sprintf(text,"%d",thisVal);
      gtk_label_set_text(GTK_LABEL(newVal),text);
      sprintf(text,"%d",thisMin);
      gtk_label_set_text(GTK_LABEL(newMin),text);
      sprintf(text,"%d",thisMax);
      gtk_label_set_text(GTK_LABEL(newMax),text);

      gtk_widget_set_sensitive(add_b,TRUE);
      gtk_widget_set_sensitive(update_b,FALSE);
      gtk_widget_set_sensitive(delete_b,FALSE);
      break;

    case UPDATE_CURRENT:
      sprintf(text,"%d",thisVal);
      gtk_label_set_text(GTK_LABEL(newVal),text);
      sprintf(text,"%d",thisMin);
      gtk_label_set_text(GTK_LABEL(newMin),text);
      sprintf(text,"%d",thisMax);
      gtk_label_set_text(GTK_LABEL(newMax),text);
      break;

    case UPDATE_EXISTING:
      switch(thisEvent) {
        case EVENT_NONE:
          gtk_label_set_text(GTK_LABEL(newEvent),"NONE");
          break;
        case MIDI_NOTE:
          gtk_label_set_text(GTK_LABEL(newEvent),"NOTE");
          break;
        case MIDI_CTRL:
          gtk_label_set_text(GTK_LABEL(newEvent),"CTRL");
          break;
        case MIDI_PITCH:
          gtk_label_set_text(GTK_LABEL(newEvent),"PITCH");
          break;
      }
      if (thisChannel >= 0) {
        sprintf(text,"%d",thisChannel);
      } else {
        sprintf(text,"%s","Any");
      }
      gtk_label_set_text(GTK_LABEL(newChannel),text);
      sprintf(text,"%d",thisNote);
      gtk_label_set_text(GTK_LABEL(newNote),text);
      gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(newType));
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(newType),NULL,"NONE");
      switch(thisEvent) {
        case EVENT_NONE:
	  gtk_combo_box_set_active (GTK_COMBO_BOX(newType),0);
          break;
        case MIDI_NOTE:
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(newType),NULL,"KEY");
	  if(thisType==TYPE_NONE) {
	    gtk_combo_box_set_active (GTK_COMBO_BOX(newType),0);
	  } else if(thisType==MIDI_KEY) {
	    gtk_combo_box_set_active (GTK_COMBO_BOX(newType),1);
	  }
          break;
        case MIDI_CTRL:
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(newType),NULL,"KNOB/SLIDER");
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(newType),NULL,"WHEEL");
	  if(thisType==TYPE_NONE) {
	    gtk_combo_box_set_active (GTK_COMBO_BOX(newType),0);
	  } else if(thisType==MIDI_KNOB) {
	    gtk_combo_box_set_active (GTK_COMBO_BOX(newType),1);
	  } else if(thisType==MIDI_WHEEL) {
            gtk_combo_box_set_active (GTK_COMBO_BOX(newType),2);
          }
          break;
        case MIDI_PITCH:
          gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(newType),NULL,"KNOB/SLIDER");
	  if(thisType==MIDI_KNOB) {
	    gtk_combo_box_set_active (GTK_COMBO_BOX(newType),1);
	  } else {
	    gtk_combo_box_set_active (GTK_COMBO_BOX(newType),0);
          }
      }
      sprintf(text,"%d",thisVal);
      gtk_label_set_text(GTK_LABEL(newVal),text);
      sprintf(text,"%d",thisMin);
      gtk_label_set_text(GTK_LABEL(newMin),text);
      sprintf(text,"%d",thisMax);
      gtk_label_set_text(GTK_LABEL(newMax),text);
  
      find_current_cmd();
      g_print("%s: current_cmd %p\n",__FUNCTION__,current_cmd);

      if (current_cmd != NULL) {
        thisDelay = current_cmd->delay;
        thisVfl1  = current_cmd->vfl1;
        thisVfl2  = current_cmd->vfl2;
        thisFl1   = current_cmd->fl1;
        thisFl2   = current_cmd->fl2;
        thisLft1  = current_cmd->lft1;
        thisLft2  = current_cmd->lft2;
        thisRgt1  = current_cmd->rgt1;
        thisRgt2  = current_cmd->rgt2;
        thisFr1   = current_cmd->fr1;
        thisFr2   = current_cmd->fr2;
        thisVfr1  = current_cmd->vfr1;
        thisVfr2  = current_cmd->vfr2;
      }
      update_wheelparams(NULL);
      gtk_widget_set_sensitive(add_b,FALSE);
      gtk_widget_set_sensitive(update_b,TRUE);
      gtk_widget_set_sensitive(delete_b,TRUE);
      break;

  }

  return 0;
}

typedef struct MYEVENT {
  enum MIDIevent event;
  int            channel;
  int            note;
  int            val;
} myevent;

int ProcessNewMidiConfigureEvent(void * data) {

  //
  // This is now running in the GTK idle queue
  //

  myevent *mydata = (myevent *) data;
  enum MIDIevent event=mydata->event;
  int  channel = mydata->channel;
  int  note = mydata->note;
  int  val = mydata->val;

  gboolean valid;
  char *str_event;
  char *str_channel;
  char *str_note;
  char *str_type;
  char *str_action;
  int i;

  gint tree_event;
  gint tree_channel;
  gint tree_note;

  g_free(data);

  if(event==thisEvent && channel==thisChannel && note==thisNote) {
    thisVal=val;
    if(val<thisMin) thisMin=val;
    if(val>thisMax) thisMax=val;
    update(GINT_TO_POINTER(UPDATE_CURRENT));
  } else {
    thisEvent=event;
    thisChannel=channel;
    if (accept_any) thisChannel=-1;
    thisNote=note;
    thisVal=val;
    thisMin=val;
    thisMax=val;
    thisType=TYPE_NONE;
    thisAction=NO_ACTION;
    //
    // set default values for wheel parameters
    //
    thisDelay =  0;
    thisVfl1  = -1;
    thisVfl2  = -1;
    thisFl1   = -1;
    thisFl2   = -1;
    thisLft1  =  0;
    thisLft2  = 63;
    thisRgt1  = 65;
    thisRgt2  =127;
    thisFr1   = -1;
    thisFr2   = -1;
    thisVfr1  = -1;
    thisVfr2  = -1;

    // search tree to see if it is existing event
    valid=gtk_tree_model_get_iter_first(model,&iter);
    while(valid) {
      gtk_tree_model_get(model, &iter, EVENT_COLUMN, &str_event, -1);
      gtk_tree_model_get(model, &iter, CHANNEL_COLUMN, &str_channel, -1);
      gtk_tree_model_get(model, &iter, NOTE_COLUMN, &str_note, -1);
      gtk_tree_model_get(model, &iter, TYPE_COLUMN, &str_type, -1);
      gtk_tree_model_get(model, &iter, ACTION_COLUMN, &str_action, -1);

      if(str_event!=NULL && str_channel!=NULL && str_note!=NULL && str_type!=NULL && str_action!=NULL) {
        if(strcmp(str_event,"CTRL")==0) {
          tree_event=MIDI_CTRL;
        } else if(strcmp(str_event,"PITCH")==0) {
          tree_event=MIDI_PITCH;
        } else if(strcmp(str_event,"NOTE")==0) {
          tree_event=MIDI_NOTE;
        } else {
          tree_event=EVENT_NONE;
        }
        if (!strncmp(str_channel,"Any", 3)) {
	  tree_channel=-1;
        } else {
          tree_channel=atoi(str_channel);
        }
        tree_note=atoi(str_note);

	if(thisEvent==tree_event && thisChannel==tree_channel && thisNote==tree_note) {
          thisVal=0;
          thisMin=0;
          thisMax=0;
          if(strcmp(str_type,"KEY")==0) {
            thisType=MIDI_KEY;
          } else if(strcmp(str_type,"KNOB/SLIDER")==0) {
            thisType=MIDI_KNOB;
          } else if(strcmp(str_type,"WHEEL")==0) {
            thisType=MIDI_WHEEL;
          } else {
            thisType=TYPE_NONE;
          }
          thisAction=NO_ACTION;
          for(i=0;i<ACTIONS;i++) {
            if(strcmp(ActionTable[i].str,str_action)==0 && (ActionTable[i].type & thisType)) {
              thisAction=ActionTable[i].action;
              break;
            }
          }
	  gtk_tree_view_set_cursor(GTK_TREE_VIEW(view),gtk_tree_model_get_path(model,&iter),NULL,FALSE);
          update(GINT_TO_POINTER(UPDATE_EXISTING));
          return 0;
	}
      }

      valid=gtk_tree_model_iter_next(model,&iter);
    }
    
    update(GINT_TO_POINTER(UPDATE_NEW));
  }
  return 0;
}
 
void NewMidiConfigureEvent(enum MIDIevent event, int channel, int note, int val) {
  myevent *data = g_new(myevent, 1);
  data->event = event;
  data->channel = channel;
  data->note = note;
  data->val = val;
  //g_print("%s: Event=%d Chan=%d Note=%d Val=%d\n", __FUNCTION__, event, channel, note, val);
  g_idle_add(ProcessNewMidiConfigureEvent, data);
}

void midi_save_state() {
  char name[80];
  char value[80];
  struct desc *cmd;
  gint entry;
  int i;
  char *cp;

  entry=0;
  for (i=0; i<n_midi_devices; i++) {
    if (midi_devices[i].active) {
      sprintf(name,"mididevice[%d].name",entry);
      setProperty(name, midi_devices[i].name);
      entry++;
    }
  }

    // the value i=128 is for the PitchBend
    for(i=0;i<129;i++) {
      cmd=MidiCommandsTable[i];
      entry=-1;
      while(cmd!=NULL) {
        entry++;
        //g_print("%s:  channel=%d key=%d entry=%d event=%s type=%s action=%s\n",__FUNCTION__,cmd->channel,i,entry,midi_events[cmd->event],midi_types[cmd->type],ActionTable[cmd->action].str);

        sprintf(name,"midi[%d].entry[%d].channel",i,entry);
        sprintf(value,"%d",cmd->channel);
        setProperty(name,value);

        sprintf(name,"midi[%d].entry[%d].channel[%d].event",i,entry,cmd->channel);
        sprintf(value,"%s",midi_events[cmd->event]);
        setProperty(name,value);
        sprintf(name,"midi[%d].entry[%d].channel[%d].action",i,entry,cmd->channel);
        sprintf(value,"%s",ActionTable[cmd->action].str);
        //
        // ActionTable strings may contain '\n', convert this to '$'
        //
        cp=value;
        while (*cp ) {
          if (*cp == '\n') *cp='$';
          cp++;
        }
        setProperty(name,value);
        sprintf(name,"midi[%d].entry[%d].channel[%d].type",i,entry,cmd->channel);
        sprintf(value,"%s",midi_types[cmd->type]);
        setProperty(name,value);

        //
        // For wheels, also store the additional parameters,
        // but do so only if they deviate from the default values.
        //
        if (cmd->type == MIDI_WHEEL) {
          if (cmd->delay > 0) {
            sprintf(name,"midi[%d].entry[%d].channel[%d].delay",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->delay);
            setProperty(name, value);
          }
          if (cmd->vfl1 != -1 || cmd->vfl2 != -1) {
            sprintf(name,"midi[%d].entry[%d].channel[%d].vfl1",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->vfl1);
            setProperty(name, value);
            sprintf(name,"midi[%d].entry[%d].channel[%d].vfl2",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->vfl2);
            setProperty(name, value);
          }
          if (cmd->fl1 != -1 || cmd->fl2 != -1) {
            sprintf(name,"midi[%d].entry[%d].channel[%d].fl1",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->fl1);
            setProperty(name, value);
            sprintf(name,"midi[%d].entry[%d].channel[%d].fl2",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->fl2);
            setProperty(name, value);
          }
          if (cmd->lft1 != 0  || cmd->lft2 != 63) {
            sprintf(name,"midi[%d].entry[%d].channel[%d].lft1",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->lft1);
            setProperty(name, value);
            sprintf(name,"midi[%d].entry[%d].channel[%d].lft2",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->lft2);
            setProperty(name, value);
          }
          if (cmd->rgt1 != 65 || cmd->rgt2 != 127) {
            sprintf(name,"midi[%d].entry[%d].channel[%d].rgt1",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->rgt1);
            setProperty(name, value);
            sprintf(name,"midi[%d].entry[%d].channel[%d].rgt2",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->rgt2);
            setProperty(name, value);
          }
          if (cmd->fr1 != -1 || cmd->fr2 != -1) {
            sprintf(name,"midi[%d].entry[%d].channel[%d].fr1",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->fr1);
            setProperty(name, value);
            sprintf(name,"midi[%d].entry[%d].channel[%d].fr2",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->fr2);
            setProperty(name, value);
          }
          if (cmd->vfr1 != -1 || cmd->vfr2 != -1) {
            sprintf(name,"midi[%d].entry[%d].channel[%d].vfr1",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->vfr1);
            setProperty(name, value);
            sprintf(name,"midi[%d].entry[%d].channel[%d].vfr2",i,entry,cmd->channel);
            sprintf(value,"%d",cmd->vfr2);
            setProperty(name, value);
          }
        }

        cmd=cmd->next;
      }
      if(entry!=-1) {
        sprintf(name,"midi[%d].entries",i);
        sprintf(value,"%d",entry+1);
        setProperty(name,value);
      }
    }
}

void midi_restore_state() {
  char name[80];
  char *value;
  gint entries;
  gint channel;
  gint event;
  gint type;
  gint action;
  gint delay;
  gint vfl1, vfl2;
  gint fl1, fl2;
  gint lft1, lft2;
  gint rgt1, rgt2;
  gint fr1, fr2;
  gint vfr1, vfr2;
  int i,j;
  char *cp;

  get_midi_devices();
  MidiReleaseCommands();

  //g_print("%s\n",__FUNCTION__);

  //
  // Note this is too early to open the MIDI devices, since the
  // radio has not yet fully been configured. Therefore, only
  // set the "active" flag, and the devices will be opened in
  // radio.c when it is appropriate
  //
  for(i=0; i<MAX_MIDI_DEVICES; i++) {
    sprintf(name,"mididevice[%d].name",i);
    value=getProperty(name);
    if (value) {
      for (j=0; j<n_midi_devices; j++) {
        if(strcmp(midi_devices[j].name,value)==0) {
          midi_devices[j].active=1;
          g_print("%s: mark device %s at %d as active\n",__FUNCTION__,value,j);
        }
      }
    }
  }

  // the value i=128 is for the PitchBend
  for(i=0;i<129;i++) {
    sprintf(name,"midi[%d].entries",i);
    value=getProperty(name);
    if(value) {
      entries=atoi(value);
      for(int entry=0;entry<entries;entry++) {
        sprintf(name,"midi[%d].entry[%d].channel",i,entry);
        value=getProperty(name);
        if(value) {
	  channel=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].event",i,entry,channel);
          value=getProperty(name);
	  event=EVENT_NONE;
          if(value) {
            for(j=0;j<4;j++) {
	      if(strcmp(value,midi_events[j])==0) {
		event=j;
		break;
              }
	    }
	  }
          sprintf(name,"midi[%d].entry[%d].channel[%d].type",i,entry,channel);
          value=getProperty(name);
	  type=TYPE_NONE;
          if(value) {
            for(j=0;j<5;j++) {
              if(strcmp(value,midi_types[j])==0) {
                type=j;
                break;
              }
            }
	  }
          sprintf(name,"midi[%d].entry[%d].channel[%d].action",i,entry,channel);
          value=getProperty(name);
          // convert '$' back to '\n' in action name before comparing
          cp=value;
          while (*cp) {
            if (*cp == '$') *cp='\n';
            cp++;
          }
	  action=NO_ACTION;
          if(value) {
	    for(j=0;j<ACTIONS;j++) {
              if(strcmp(value,ActionTable[j].str)==0) {
                action=ActionTable[j].action;
		break;
              }
	    }
	  }
          //
          // Look for "wheel" parameters. For those not found,
          // use default value
          //
          sprintf(name,"midi[%d].entry[%d].channel[%d].delay",i,entry,channel);
          value=getProperty(name);
          delay=0;
          if (value) delay=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].vfl1",i,entry,channel);
          value=getProperty(name);
          vfl1=-1;
          if (value) vfl1=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].vfl2",i,entry,channel);
          value=getProperty(name);
          vfl2=-1;
          if (value) vfl2=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].fl1",i,entry,channel);
          value=getProperty(name);
          fl1=-1;
          if (value) fl1=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].fl2",i,entry,channel);
          value=getProperty(name);
          fl2=-1;
          if (value) fl2=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].lft1",i,entry,channel);
          value=getProperty(name);
          lft1=0;
          if (value) lft1=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].lft2",i,entry,channel);
          value=getProperty(name);
          lft2=63;
          if (value) lft2=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].rgt1",i,entry,channel);
          value=getProperty(name);
          rgt1=65;
          if (value) rgt1=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].rgt2",i,entry,channel);
          value=getProperty(name);
          rgt2=127;
          if (value) rgt2=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].fr1",i,entry,channel);
          value=getProperty(name);
          fr1=-1;
          if (value) fr1=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].fr2",i,entry,channel);
          value=getProperty(name);
          fr2=-1;
          if (value) fr2=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].vfr1",i,entry,channel);
          value=getProperty(name);
          vfr1=-1;
          if (value) vfr1=atoi(value);
          sprintf(name,"midi[%d].entry[%d].channel[%d].vfr2",i,entry,channel);
          value=getProperty(name);
          vfr2=-1;
          if (value) vfr2=atoi(value);

          //
          // Construct descriptor and add to the list of MIDI commands
          //
	  struct desc *desc = (struct desc *) malloc(sizeof(struct desc));

          desc->next     = NULL;
          desc->action   = action; // MIDIaction
          desc->type     = type;   // MIDItype
          desc->event    = event;  // MIDevent
          desc->delay    = delay;
          desc->vfl1     = vfl1;
          desc->vfl2     = vfl2;
          desc->fl1      = fl1;
          desc->fl2      = fl2;
          desc->lft1     = lft1;
          desc->lft2     = lft2;
          desc->rgt1     = rgt1;
          desc->rgt2     = rgt2;
          desc->fr1      = fr1;
          desc->fr2      = fr2;
          desc->vfr1     = vfr1;
          desc->vfr2     = vfr2;
          desc->channel  = channel;

          MidiAddCommand(i, desc);
        }
      }
    }
  }

}

