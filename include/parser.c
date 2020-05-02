/* Name, parser.c, CS 24000, Spring 2020
 * Last updated March 27, 2020
 */

/* Add any includes here */

#include "parser.h"

#include <malloc.h>
#include <string.h>
#include <assert.h>

#define MTHD "MThd"
#define MTRK "MTrk"
#define HEADER_LENGTH (0x06000000)

uint8_t g_last_status = 0;

/*
 * Parses the given midi file
 */

song_data_t *parse_file(const char *midi_file_name) {
  assert(midi_file_name != NULL);
  FILE *file = fopen(midi_file_name, "r");
  assert(file != NULL);
  song_data_t *song_data = malloc(sizeof(song_data_t));
  assert(song_data);
  song_data->track_list = NULL;
  song_data->path = malloc((strlen(midi_file_name) + 1) * sizeof(char));
  assert(song_data->path);
  strcpy(song_data->path, midi_file_name);
  song_data->track_list = NULL;
  parse_header(file, song_data);
  for (int i = 0; i < song_data->num_tracks; i++) {
    parse_track(file, song_data);
  }
  assert(getc(file) == EOF);
  fclose(file);
  file = NULL;
  return song_data;
} /* parse_file() */

/*
 * Parses the header of the file into the given song_data_t
 */

void parse_header(FILE *file, song_data_t *song_data) {
  char chunk_type[4];
  assert(chunk_type);
  int fread_return = fread(chunk_type, sizeof(char), 4, file);
  if (fread_return != strlen(MTHD)) {
    return;
  }
  assert(strcmp(chunk_type, MTHD) == 0);
  uint32_t length = 0;
  fread_return = fread(&length, sizeof(uint32_t), 1, file);
  if (fread_return != 1) {
    return;
  }
  //printf("%x\n", length);
  assert(length == HEADER_LENGTH);
  uint16_t format = 0x0;
  //fseek(file, 1, SEEK_CUR);
  fread_return = fread(&format, sizeof(uint16_t), 1, file);
  if (fread_return != 1) {
    return;
  }
  format = (format >> 8) | (format << 8);
  assert(format <= 0x02);
  song_data->format = (uint8_t) format;
  uint16_t num_tracks = 0x00;
  fread_return = fread(&num_tracks, sizeof(uint16_t), 1, file);
  if (fread_return != 1) {
    return;
  }
  num_tracks = (num_tracks >> 8) | (num_tracks << 8);
  song_data->num_tracks = num_tracks;
  uint16_t division = 0;
  fread_return = fread(&division, sizeof(uint16_t), 1, file);
  if (fread_return != 1) {
    return;
  }
  division = (division >> 8) | (division << 8);
  song_data->division.uses_tpq = !(division & (1 << 8) >> 8);
  if (song_data->division.uses_tpq) {
    song_data->division.ticks_per_qtr = division;
  }
  else {
    song_data->division.frames_per_sec = ((1 << 7) - 1) & (division >> 8);
    song_data->division.ticks_per_frame = ((1 << 8) - 1) & (division >> 0);
  }

} /* parse_header() */

/*
 * parses tracks from a given file
 */

void parse_track(FILE *file, song_data_t *song_data) {
  int counter = 0;
  char type[4];
  int fread_return = fread(type, sizeof(char), strlen(MTRK), file);
  if (fread_return != strlen(MTRK)) {
    return;
  }
  assert(strcmp(type, MTRK) == 0);
  track_node_t *track_node = malloc(sizeof(track_node_t));
  assert(track_node);
  track_t *track = malloc(sizeof(track_t));
  assert(track);
  uint32_t length = 0x00;
  fread_return = fread(&length, sizeof(uint32_t), 1, file);
  if (fread_return != 1) {
    return;
  }
  uint32_t len = ((length >> 24) & 0xff) | ((length << 8) & 0xff0000) |
    ((length >> 8) & 0xff00) | ((length << 24) & 0xff000000);
  length = len;
  assert(length > 0);
  track->length = length;
  track->event_list = malloc(sizeof(event_node_t));
  assert(track->event_list);
  track->event_list->event = NULL;
  int end_position = ftell(file) + length;
  track->event_list->event = parse_event(file);
  track->event_list->next_event = NULL;
  while (ftell(file) < end_position) {
    event_node_t *list = track->event_list;
    while (list->next_event != NULL) {
      list = list->next_event;
    }
    list->next_event = malloc(sizeof(event_node_t));
    assert(list->next_event);
    list->next_event->event = parse_event(file);
    list->next_event->next_event = NULL;
    counter++;
  }
  track_node->next_track = NULL;
  track_node->track = track;
  if (song_data->track_list == NULL) {
    song_data->track_list = track_node;
    return;
  }
  track_node_t *tracks = song_data->track_list;
  while (tracks->next_track) {
    tracks = tracks->next_track;
  }
  tracks->next_track = track_node;
} /* parse_track() */

/*
 * general function to parse events from a midi file
 */

event_t *parse_event(FILE *file) {
  event_t *event = malloc(sizeof(event_t));
  assert(event);
  event->delta_time = parse_var_len(file);
  int fread_return = fread(&(event->type), sizeof(uint8_t), 1, file);
  if (fread_return != 1) {
    return NULL;
  }
  if (event->type == META_EVENT) {
    event->meta_event = parse_meta_event(file);
  }
  else if ((event->type == SYS_EVENT_1) || (event->type == SYS_EVENT_2)) {
    event->sys_event = parse_sys_event(file, event->type);
  }
  else {
    event->midi_event = parse_midi_event(file, event->type);
  }
  return event;
} /* parse_event() */

/*
 * parses a system event
 */

sys_event_t parse_sys_event(FILE *file, uint8_t type) {
  sys_event_t event = {};
  event.data_len = parse_var_len(file);
  if (event.data_len) {
    event.data = malloc(event.data_len);
    assert(event.data);
    int fread_return = fread(event.data, sizeof(uint8_t), event.data_len
        / sizeof(uint8_t), file);
    if (fread_return != (event.data_len / sizeof(uint8_t))) {
      return event;
    }
  }
  return event;
} /* parse_sys_event() */

/*
 * parses a meta event
 */

meta_event_t parse_meta_event(FILE *file) {
  meta_event_t event = {};
  uint8_t type = 0;
  int fread_return = fread(&type, sizeof(uint8_t), 1, file);
  assert(fread_return == 1);
  event.name = META_TABLE[type].name;
  assert(event.name != NULL);
  event.data_len = META_TABLE[type].data_len;
  event.data = META_TABLE[type].data;
  if (event.data_len == 0) {
    event.data_len = parse_var_len(file);
  }
  else {
    fread_return = fread(&event.data_len, sizeof(uint8_t), 1, file);
    assert(fread_return == 1);
    assert(event.data_len == META_TABLE[type].data_len);
  }
  if (event.data_len) {
    event.data = malloc(event.data_len);
    assert(event.data);
    fread_return = fread(event.data, sizeof(uint8_t), event.data_len /
        sizeof(uint8_t), file);
    assert(fread_return == event.data_len / sizeof(uint8_t));
  }
  return event;
} /* parse_meta_event() */

/*
 * parses the midi event
 */

midi_event_t parse_midi_event(FILE *file, uint8_t status) {
  midi_event_t event = {};
  event.status = status;
  if ((event.status) & (1 << 7)) {
    g_last_status = event.status;
  }
  else {
    event.status = g_last_status;
    fseek(file, ftell(file) - 1, SEEK_SET);
  }
  event.name = MIDI_TABLE[event.status].name;
  assert(event.name != NULL);
  event.data_len = MIDI_TABLE[event.status].data_len;
  if (event.data_len) {
    event.data = malloc(event.data_len);
    assert(event.data);
    int fread_return = fread(event.data, sizeof(uint8_t), event.data_len /
                       sizeof(uint8_t), file);
    assert(fread_return == event.data_len / sizeof(uint8_t));
  }
  return event;
} /* parse_midi_event() */

/*
 * parse a variable length quantity from a midi file
 */

uint32_t parse_var_len(FILE *file) {
  uint8_t read = 0;
  int fread_return = fread(&read, sizeof(uint8_t), 1, file);
  assert(fread_return == 1);
  uint32_t parsed_num = read & (~(1 << 7));
  while (read & (1 << 7)) {
    parsed_num = parsed_num << 7;
    fread_return = fread(&read, sizeof(uint8_t), 1, file);
    assert(fread_return == 1);
    parsed_num = parsed_num | (read & (~(1 << 7)));
  }
  return parsed_num;
} /* parse_var_len() */

/*
 * returns the type of event the argument is
 */

uint8_t event_type(event_t *event) {
  if (event->type == META_EVENT) {
    return META_EVENT_T;
  }
  else if ((event->type == SYS_EVENT_1) || (event->type == SYS_EVENT_2)) {
    return SYS_EVENT_T;
  }
  return MIDI_EVENT_T;
} /* event_type() */

/*
 * frees the memory associated with a song_data_t struct
 */

void free_song(song_data_t *song_data) {
  int counter = 0;
  while (song_data->track_list) {
    counter++;
    track_node_t *track_node = song_data->track_list;
    song_data->track_list = song_data->track_list->next_track;
    free_track_node(track_node);
  }
  free(song_data->track_list);
  song_data->track_list = NULL;
  free(song_data->path);
  song_data->path = NULL;
  free(song_data);
  song_data = NULL;
} /* free_song() */

/*
 * frees the memory associated with a track_node_t struct
 */

void free_track_node(track_node_t *node) {
  int counter = 0;
  while (node->track->event_list) {
    event_node_t *event_node = node->track->event_list;
    node->track->event_list = node->track->event_list->next_event;
    free_event_node(event_node);
    counter++;
  }
  free(node->track);
  node->track = NULL;
  free(node);
  node = NULL;
} /* free_track_node() */

/*
 * frees the memory associated with a event_node_t struct
 */

void free_event_node(event_node_t *node) {
  uint8_t type = event_type(node->event);
  if (type == META_EVENT_T) {
    if (node->event->meta_event.data_len) {
      free(node->event->meta_event.data);
      node->event->meta_event.data = NULL;
    }
  }
  else if (type == SYS_EVENT_T) {
    if (node->event->sys_event.data_len) {
      free(node->event->sys_event.data);
      node->event->sys_event.data = NULL;
    }
  }
  else {
    if (node->event->midi_event.data_len) {
      free(node->event->midi_event.data);
      node->event->midi_event.data = NULL;
    }
  }
  free(node->event);
  node->event = NULL;
  free(node);
  node = NULL;
} /* free_event_node() */

/*
 * swaps the endianness of a given 16 bit int
 */

uint16_t end_swap_16(uint8_t num[2]) {
  uint16_t little_end = 0;
  little_end = num[1];
  little_end = num[1] << 7;
  little_end = little_end | num[0];
  return little_end;
} /* end_swap_16() */

/*
 * swaps the endianness of a 32 bit int
 */

uint32_t end_swap_32(uint8_t num[4]) {
  uint32_t little_end = 0;
  uint8_t two_nums[2];
  two_nums[0] = num[0];
  two_nums[1] = num[1];
  uint16_t half = end_swap_16(two_nums);
  little_end = half;
  little_end = little_end << 15;
  two_nums[0] = num[2];
  two_nums[1] = num[3];
  half = end_swap_16(two_nums);
  little_end = little_end | half;
  return little_end;
} /* end_swap_32() */
