/* Name, alterations.c, CS 24000, Spring 2020
 * Last updated April 9, 2020
 */

/* Add any includes here */

#include "alterations.h"

#include <assert.h>
#include <stdlib.h>

#define MODIFIED (1)
#define FAIL (0)
#define NOTE_MAX (127)
#define NOTE_MIN (0)
#define CHANNEL_MAX (15)
#define MIDI_MIN (0x80)
#define MIDI_MAX (0xFE)
#define CLEAR_FOUR_MASK (0xF0)
#define NOTE_EVENT_MAX (0xAF)
#define VLQ_1_BYTE_MAX (0x7F)
#define VLQ_2_BYTE_MAX (0x3FFF)
#define VLQ_3_BYTE_MAX  (0x1FFFFF)
#define PROGRAM_CHANGE_MIN (0xC0)
#define PROGRAM_CHANGE_MAX (0xCF)

int octave_helper(event_t *event, void *data);
int time_helper(song_data_t *song, float multiplier);
int instruments_helper(event_t *event, void *remapping_table);
int notes_helper(event_t *event, void *remapping_table);
event_node_t *duplicate_events(event_node_t *list, int lowest_channel);
int vlq_size_difference(uint32_t vlq_1, uint32_t vlq_2);

/*
 * changes the octave of the event
 */

int change_event_octave(event_t *event, int *octaves) {
  assert(event);
  assert(octaves);
  if ((event->type >= MIDI_MIN) && (event->type <= NOTE_EVENT_MAX)) {
    int note_changed = event->midi_event.data[0] + (*octaves * OCTAVE_STEP);
    if ((note_changed >= NOTE_MIN) && (note_changed <= NOTE_MAX)) {
      event->midi_event.data[0] = (uint8_t) note_changed;
      return MODIFIED;
    }
  }
  return FAIL;
} /* change_event_octave() */

/*
 * scales the delta-time of an event
 */

int change_event_time(event_t *event, float *multiplier) {
  assert(event);
  assert(multiplier);
  uint32_t old_delta = event->delta_time;
  event->delta_time = event->delta_time * *multiplier;
  return vlq_size_difference(old_delta, event->delta_time);
} /* change_event_time() */

/*
 * calculates the difference in bytes of 2 values when stored as VLQs
 */

int vlq_size_difference(uint32_t vlq_1, uint32_t vlq_2) {
  int old_byte = 0;
  if (vlq_1 <= VLQ_1_BYTE_MAX) {
    old_byte = 1;
  }
  else if (vlq_1 <= VLQ_2_BYTE_MAX) {
    old_byte = 2;
  }
  else if (vlq_1 <= VLQ_3_BYTE_MAX) {
    old_byte = 3;
  }
  else {
    old_byte = 4;
  }
  int new_byte = 0;
  if (vlq_2 <= VLQ_1_BYTE_MAX) {
    new_byte = 1;
  }
  else if (vlq_2 <= VLQ_2_BYTE_MAX) {
    new_byte = 2;
  }
  else if (vlq_2 <= VLQ_3_BYTE_MAX) {
    new_byte = 3;
  }
  else {
    new_byte = 4;
  }
  return new_byte - old_byte;
} /* vlq_size_difference() */

/*
 * changes the instrument based on a remapping
 */

int change_event_instrument(event_t *event, remapping_t table) {
  assert(event);
  if ((event->type >= PROGRAM_CHANGE_MIN) && (event->type <=
        PROGRAM_CHANGE_MAX)) {
    event->midi_event.data[0] = table[event->midi_event.data[0]];
    return MODIFIED;
  }
  return FAIL;
} /* change_event_instrument() */

/*
 * changes the note of the current event
 */

int change_event_note(event_t *event, remapping_t table) {
  assert(event);
  if ((event->type >= MIDI_MIN) && (event->type <= NOTE_EVENT_MAX)) {
    event->midi_event.data[0] = table[event->midi_event.data[0]];
    return MODIFIED;
  }
  return FAIL;
} /* change_event_note() */

/*
 * applies the given function to every event in the song with "data"
 */

int apply_to_events(song_data_t *song, event_func_t function, void *data) {
  assert(song);
  assert(function);
  int function_return = 0;
  track_node_t *track_list = song->track_list;
  while (track_list) {
    event_node_t *event_list = track_list->track->event_list;
    while (event_list) {
      function_return += function(event_list->event, data);
      event_list = event_list->next_event;
    }
    track_list = track_list->next_track;
  }
  return function_return;
} /* apply_to_events() */

/*
 * changes the octave for the entire song
 */

int change_octave(song_data_t *song, int octave_change) {
  return apply_to_events(song, octave_helper, (void *) &octave_change);
} /* change_octave() */

/*
 * casts the arguments to required type for change_octave
 */

int octave_helper(event_t *event, void *octave) {
  return change_event_octave(event, (int *) octave);
} /* octave_helper() */

/*
 * modifies the length of the song by the multiplier
 */

int warp_time(song_data_t *song, float multiplier) {
  int size_difference = time_helper(song, multiplier);
  return size_difference;
} /* warp_time() */

/*
 * casts the arguments to the required type for change_event_time
 */

int time_helper(song_data_t *song, float multiplier) {
  assert(song);
  track_node_t *track_list = song->track_list;
  int total_change = 0;
  while (track_list) {
    int track_length = 0;
    event_node_t *event_list = track_list->track->event_list;
    while (event_list) {
      track_length += change_event_time(event_list->event, &multiplier);
      event_list = event_list->next_event;
    }
    track_list->track->length += track_length;
    total_change += track_length;
    track_list = track_list->next_track;
  }
  return total_change;
} /* time_helper() */

/*
 * remaps all the instruments according to the table
 */

int remap_instruments(song_data_t *song, remapping_t table) {
  return apply_to_events(song, instruments_helper, (void *) table);
} /* remap_instruments() */

/*
 * casts the arguments to the required type for change_event_instrument
 */

int instruments_helper(event_t *event, void *table) {
  return change_event_instrument(event, table);
} /* instruments_helper() */

/*
 * remaps the notes according to the table
 */

int remap_notes(song_data_t *song, remapping_t table) {
  return apply_to_events(song, notes_helper, table);
} /* remap_notes() */

/*
 * casts the arguments to required type for change_event_note
 */

int notes_helper(event_t *event, void *table) {
  return change_event_note(event, table);
} /* notes_helper() */

/*
 * adds the track at the given index wtih the given changes to the end of the
 * track list
 */

void add_round(song_data_t *song, int track_index, int octave_difference,
    unsigned int time_delay, uint8_t instrument) {
  assert(song);
  assert(track_index < song->num_tracks);
  assert(song->format != 2);
  track_node_t *round = song->track_list;
  for (int i = 0; i < track_index; i++) {
    round = round->next_track;
  }
  int channels[CHANNEL_MAX + 1];
  for (int i = 0; i <= CHANNEL_MAX; i++) {
    channels[i] = 0;
  }
  event_node_t *events = round->track->event_list;
  while (events) {
    if ((events->event->type >= MIDI_MIN) && (events->event->type <=
          MIDI_MAX)) {
      channels[events->event->type & (~CLEAR_FOUR_MASK)] = 1;
    }
    events = events->next_event;
  }
  int lowest_channel = CHANNEL_MAX + 1;
  for (int i = 0; i <= CHANNEL_MAX; i++) {
    if (!channels[i]) {
      lowest_channel = i;
      break;
    }
  }
  assert(lowest_channel <= CHANNEL_MAX);
  track_node_t *duplicate = malloc(sizeof(track_node_t));
  assert(duplicate);
  duplicate->track = malloc(sizeof(track_t));
  assert(duplicate->track);
  duplicate->next_track = NULL;
  printf("%x\n", lowest_channel);
  //printf("%x\n", round->track->event_list->event->type);
  duplicate->track->event_list = duplicate_events(round->track->event_list,
      lowest_channel);
  song_data_t *temp_song = malloc(sizeof(song_data_t));
  temp_song->track_list = duplicate;
  change_octave(temp_song, octave_difference);
  remapping_t instrument_change = {0};
  for (int i = 0; i <= 0xFF; i++) {
    instrument_change[i] = instrument;
  }
  remap_instruments(temp_song, instrument_change);
  free(temp_song);
  temp_song = NULL;
  uint32_t old_delta = duplicate->track->event_list->event->delta_time;
  duplicate->track->event_list->event->delta_time += time_delay;
  duplicate->track->length = vlq_size_difference(old_delta,
      duplicate->track->event_list->event->delta_time);
  printf("%d\n", duplicate->track->event_list->event->delta_time);
} /* add_round() */

/*
 * duplicates the events from a given event list
 */

event_node_t *duplicate_events(event_node_t *list, int lowest_channel) {
  if (list == NULL) {
    return NULL;
  }
  event_node_t *event = malloc(sizeof(event_node_t));
  assert(event);
  event->event = malloc(sizeof(event_t));
  assert(event->event);
  event->event = list->event;
  //printf("%x\n", event->event->type);
  if (event->event->type == META_EVENT) {
    if (event->event->meta_event.data_len) {
      event->event->meta_event.data = malloc(sizeof(uint8_t) *
          event->event->meta_event.data_len);
      assert(event->event->meta_event.data);
      for (int i = 0; i < event->event->meta_event.data_len; i++) {
        event->event->meta_event.data[i] = list->event->meta_event.data[i];
      }
    }
  }
  else if ((event->event->type == SYS_EVENT_1) || (event->event->type ==
        SYS_EVENT_2)) {
    if (event->event->sys_event.data_len) {
      event->event->sys_event.data = malloc(sizeof(uint8_t) *
          event->event->sys_event.data_len);
      assert(event->event->sys_event.data);
      for (int i = 0; i < event->event->sys_event.data_len; i++) {
        event->event->sys_event.data[i] = list->event->sys_event.data[i];
      }
    }
  }
  else {
    event->event->type = ((CLEAR_FOUR_MASK & event->event->type) |
        lowest_channel);
    //printf("%x\n", event->event->type);
    if (event->event->midi_event.data_len) {
      event->event->midi_event.data = malloc(sizeof(uint8_t) *
          event->event->midi_event.data_len);
      assert(event->event->midi_event.data);
      for (int i = 0; i < event->event->midi_event.data_len; i++) {
        event->event->midi_event.data[i] = list->event->midi_event.data[i];
      }
    }
  }
  event->next_event = duplicate_events(list->next_event, lowest_channel);
  return event;
} /* duplicate_events() */

/*
 * Function called prior to main that sets up random mapping tables
 */

void build_mapping_tables()
{
  for (int i = 0; i <= 0xFF; i++) {
    I_BRASS_BAND[i] = 61;
  }

  for (int i = 0; i <= 0xFF; i++) {
    I_HELICOPTER[i] = 125;
  }

  for (int i = 0; i <= 0xFF; i++) {
    N_LOWER[i] = i;
  }
  //  Swap C# for C
  for (int i = 1; i <= 0xFF; i += 12) {
    N_LOWER[i] = i - 1;
  }
  //  Swap F# for G
  for (int i = 6; i <= 0xFF; i += 12) {
    N_LOWER[i] = i + 1;
  }
} /* build_mapping_tables() */
