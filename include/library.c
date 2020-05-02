/* Name, library.c, CS 24000, Spring 2020
 * Last updated March 27, 2020
 */

/* Add any includes here */

#include "library.h"

#include <string.h>
#include <assert.h>
#include <malloc.h>
#include <ftw.h>
#include <limits.h>

#define ERROR (-1)
#define OK (0)
#define NO_DIRS (5)

tree_node_t *g_song_library = NULL;

int ftw_callback(const char *file_path, const struct stat *ptr, int flag);

/*
 * returns the parent's branch pointting to a node with the given song_name
 */

tree_node_t **find_parent_pointer(tree_node_t **root, const char *song_name) {
  tree_node_t *tree = *root;
  tree_node_t *parent = *root;
  if (strcmp(tree->song_name, song_name) == 0) {
    return root;
  }
  while (tree) {
    if (strcmp(tree->song_name, song_name) > 0) {
      parent = tree;
      tree = tree->left_child;
    }
    else if (strcmp(tree->song_name, song_name) < 0) {
      parent = tree;
      tree = tree->right_child;
    }
    else {
      if (parent->left_child) {
        if (strcmp(parent->left_child->song_name, song_name) == 0) {
          return &(parent->left_child);
        }
      }
      return &(parent->right_child);
    }
  }
  return NULL;
} /* find_parent_pointer() */

/*
 * inserts the node into the given tree
 */

int tree_insert(tree_node_t **root, tree_node_t *node) {
  tree_node_t *tree = *root;
  if (tree == NULL) {
    *root = node;
    return INSERT_SUCCESS;
  }
  while (1) {
    if (strcmp(tree->song_name, node->song_name) == 0) {
      return DUPLICATE_SONG;
    }
    else if (strcmp(tree->song_name, node->song_name) > 0) {
      if (tree->left_child == NULL) {
        tree->left_child = node;
        return INSERT_SUCCESS;
      }
      tree = tree->left_child;
    }
    else {
      if (tree->right_child == NULL) {
        tree->right_child = node;
        return INSERT_SUCCESS;
      }
      tree = tree->right_child;
    }
  }
  return INSERT_SUCCESS;
} /* tree_insert() */

/*
 * removes the node with the given song_name from the tree
 */

int remove_song_from_tree(tree_node_t **root, const char *song_name) {
  tree_node_t *tree = *root;
  if (strcmp(tree->song_name, song_name) == 0) {
    tree_node_t *left_child = tree->left_child;
    tree_node_t *right_child = tree->right_child;
    free_node(tree);
    if (left_child != NULL) {
      *root = left_child;
    }
    if (right_child != NULL) {
      if (*root == NULL) {
        *root = right_child;
      }
      else {
        tree_insert(root, right_child);
      }
    }
    return DELETE_SUCCESS;
  }
  while (tree) {
    if (strcmp(tree->song_name, song_name) > 0) {
      tree = tree->left_child;
    }
    else if (strcmp(tree->song_name, song_name) < 0) {
      tree = tree->right_child;
    }
    else {
      tree_node_t *left_child = tree->left_child;
      tree_node_t *right_child = tree->right_child;
      tree_node_t **parent_pointer = (find_parent_pointer(root, song_name));
      free_node(tree);
      *parent_pointer = NULL;
      if (left_child != NULL) {
        tree_insert(root, left_child);
      }
      if (right_child != NULL) {
        tree_insert(root, right_child);
      }
      return DELETE_SUCCESS;
    }
  }
  return SONG_NOT_FOUND;
} /* remove_song_from_tree() */

/*
 * frees the give node
 */

void free_node(tree_node_t *node) {
  node->left_child = NULL;
  node->right_child = NULL;
  free_song(node->song);
  free(node);
  node = NULL;
} /* free_node() */

/*
 * prints the song_name to the given file
 */

void print_node(tree_node_t *node, FILE *file) {
  fprintf(file, "%s\n", node->song_name);
  //printf("%s\n", node->song_name);
} /* print_node() */

/*
 * traverses in the pre_order from a given node pointer and calls traversal
 * and passes data to the function
 */

void traverse_pre_order(tree_node_t *pointer, void *data,
    traversal_func_t traversal) {
  if (pointer == NULL) {
    return;
  }
  traversal(pointer, data);
  traverse_pre_order(pointer->left_child, data, traversal);
  traverse_pre_order(pointer->right_child, data, traversal);
} /* traverse_pre_order() */


/*
 * traverses the tree in order from a given node pointer and calls traversal
 * and passes data to the function
 */

void traverse_in_order(tree_node_t *pointer, void *data,
    traversal_func_t traversal) {
  if (pointer == NULL) {
    return;
  }
  traverse_in_order(pointer->left_child, data, traversal);
  traversal(pointer, data);
  traverse_in_order(pointer->right_child, data, traversal);
} /* traverse_in_order() */


/*
 * traverses the tree post order from a given node pointer and calls traversal
 * and passes data to the function
 */

void traverse_post_order(tree_node_t *pointer, void *data,
    traversal_func_t traversal) {
  if (pointer == NULL) {
    return;
  }
  traverse_post_order(pointer->left_child, data, traversal);
  traverse_post_order(pointer->right_child, data, traversal);
  traversal(pointer, data);
} /* traverse_post_order() */

/*
 * frees an entire library
 */

void free_library(tree_node_t *tree) {
  if (tree == NULL) {
    return;
  }
  free_library(tree->left_child);
  free_library(tree->right_child);
  free_node(tree);
} /* free_library() */

/*
 * writes the song library to the file
 */

void write_song_list(FILE *file, tree_node_t *tree) {
  traverse_in_order(tree, file, (void *)print_node);
} /* write_song_list() */

/*
 * makes the song library from a directory
 */

void make_library(const char *directory) {
  int ftw_return = ftw(directory, ftw_callback, NO_DIRS);
  if (ftw_return != OK) {
    printf("error\n");
    return;
  }
} /* make_library() */

/*
 * the callback function for ftw
 */

int ftw_callback(const char *file_path, const struct stat *ptr, int flag) {
  if ((flag == FTW_F) || (flag == FTW_NS)) {
    if (strrchr(file_path, '.') == NULL) {
      return OK;
    }
    if (strcmp(strrchr(file_path, '.'), ".mid") == 0) {
      //printf("midi file\n");
      tree_node_t *new_node = malloc(sizeof(tree_node_t));
      assert(new_node);
      memset(new_node, 0, sizeof(tree_node_t));
      new_node->song = parse_file(file_path);
      new_node->left_child = NULL;
      new_node->right_child = NULL;
      new_node->song_name = strrchr(new_node->song->path, '/') + 1;
      assert(tree_insert(&g_song_library, new_node) != DUPLICATE_SONG);
      //printf("%d\n", new_node->song->format);
    }
  }
  return OK;
} /* ftw_callback() */
