#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

typedef struct type type;

struct type {
  type *left;
  type *right;
  int height;
  int val;
};

type *tree = 0;

/*inline int compare(type *a, type *b) {
  return a->val - b->val;
}

  inline int avl_max(int a, int b) {
    return a > b ? a : b;
  }

  inline int avl_height(type *node) {
    return node ? node->height : -1;
  }
*/
#define inline static

#define compare(a, b) ((a)->val - (b)->val)
#define avl_max(a, b) ((a) > (b) ? (a) : (b))
#define avl_height(node) ((node) ? (node)->height : -1)

  inline void avl_update_height(type *node) {
    node->height = avl_max(avl_height(node->left), avl_height(node->right)) + 1;
  }

  inline void avl_rot_left(type **pivot) {
    type *child = (*pivot)->left;
    (*pivot)->left = child->right;
    child->right = *pivot;
    avl_update_height(*pivot);
    avl_update_height(child);
    *pivot = child;
  }

  inline void avl_rot_right(type **pivot) {
    type *child = (*pivot)->right;
    (*pivot)->right = child->left;
    child->left = *pivot;
    avl_update_height(*pivot);
    avl_update_height(child);
    *pivot = child;
  }

  inline void avl_rot_left_double(type **pivot) {
    avl_rot_right(&((*pivot)->left));
    avl_rot_left(pivot);
  }

  inline void avl_rot_right_double(type **pivot) {
    avl_rot_left(&((*pivot)->right));
    avl_rot_right(pivot);
  }

  inline int avl_balance(type *node) {
    return avl_height(node->left) - avl_height(node->right);
  }

  inline void avl_rebalance(type **tree) {
    type **parent = tree;
    int balance = avl_balance(*parent);
    if (balance > 1) {
      // Need to rotate counter-clockwise
      if (avl_balance((*parent)->left) < 0) {
        avl_rot_left_double(parent);
      } else {
        avl_rot_left(parent);
      }
    } else if (balance < -1) {
      // Need to rotate clockwise
      if (avl_balance((*parent)->right) > 0) {
        avl_rot_right_double(parent);
      } else {
        avl_rot_right(parent);
      }
    }
  }

  static void avl_insert(type **tree, type *node) {
    type **parent = tree;
    if (*parent) {
      if (compare(*parent, node) < 0) {
        avl_insert(&((*parent)->left), node);
      } else {
        avl_insert(&((*parent)->right), node);
      }
      avl_update_height(*parent);
      avl_rebalance(parent);
    } else {
      node->height = 0;
      *parent = node;
    }
  }

  inline void avl_print(type *tree, int indent) {
    if (!tree) return;
    avl_print(tree->left, indent + 1);
    int j;
    for (j = 0; j < indent; j++) fprintf(stderr, " ");
    fprintf(stderr, "val=%d h=%d\n", tree->val, tree->height);
    avl_print(tree->right, indent + 1);
  }

  inline void avl_insert_random(type **tree, type *node) {
    node->val = rand();
    avl_insert(tree, node);
  }

  int main() {
    int cnt = 10000000;
    type *nodes = (type*)malloc(cnt * sizeof(type));
    memset(nodes, 0, sizeof(type) * cnt);
    int i;
    for (i = 0; i < cnt; i++) {
      avl_insert_random(&tree, nodes + i);
    }
    return 0;
  }

