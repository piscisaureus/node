#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

typedef struct ntype ntype;

# define noinline                   __attribute__ ((noinline))

struct ntype {
  ntype *left;
  ntype *right;
  int height;
  int value;
};

typedef ntype *nptr;

#define vtype int

ntype *tree = 0;

/*inline int compare(ntype *a, ntype *b) {
  return a->val - b->val;
}

  inline int avl_max(int a, int b) {
    return a > b ? a : b;
  }

  inline int avl_height(ntype *node) {
    return node ? node->height : -1;
  }
*/


#define compare(a, b) (a - b)
#define val(a) ((a)->value)
#define avl_max(a, b) ((a) > (b) ? (a) : (b))
/*#define avl_height(node) ((node) ? (node)->height : -1) */

  static inline int avl_height(ntype *node) {
    return node ? node->height : -1;
  }

  static inline void *avl_update_height(ntype *node) {
    int h1 = node->left ? node->left->height + 1 : 0;
    int h2 = node->right ? node->right->height + 1 : 0;
    node->height = h1 > h2 ? h1 : h2;
  }

  static inline void avl_rot_left(ntype **parent) {
    ntype *n1 = *parent;
    ntype *n2 = n1->left;
    n1->left = n2->right;
    n2->right = n1;
    *parent = n2;
  }

  static inline void avl_rot_right(ntype **parent) {
    ntype *n1 = *parent;
    ntype *n2 = n1->right;
    n1->right = n2->left;
    n2->left = n1;
    *parent = n2;
  }

  static inline int avl_balance(ntype *node) {
    return avl_height(node->left) - avl_height(node->right);
  }

  static void avl_rebalance(ntype **parent) {
    int balance = avl_balance(*parent);

    if (balance > 1) {
      /* Left branch is too high */
      if (avl_balance((*parent)->left) < 0) {
        /* Need to rotate the right subtree first */
        ntype **child = &((*parent)->left);
        (*child)->height --;
        (*child)->right->height ++;
        avl_rot_right(child);
      }

      /* Left rotation */
      (*parent)->height -= 2;
      avl_rot_left(parent);

    } else if (balance < -1) {
      /* Right branch is too high */
      if (avl_balance((*parent)->right) > 0) {
        /* Need to rotate the left subtree first */
        ntype **child = &((*parent)->right);
        (*child)->height --;
        (*child)->left->height ++;
        avl_rot_left(child);
      }

      /* Right rotation */
      (*parent)->height -= 2;
      avl_rot_right(parent);
    }
  }

  static void avl_insert(ntype **parent, ntype *node) {
    if (*parent) {
      if (compare(val((*parent)), val(node)) < 0) {
        avl_insert(&((*parent)->left), node);
      } else {
        avl_insert(&((*parent)->right), node);
      }
      avl_update_height(*parent);
      avl_rebalance(parent);
    } else {
      avl_update_height(node);
      *parent = node;
    }
  }

  static inline void avl_splice(ntype **node) {
    ntype *removed = *node;
    (*node) = removed->left;
    if (removed->right) {
      avl_insert(node, removed->right);
    }
    if (*node) {
      avl_update_height(*node);
      avl_rebalance(node);
    }
  }

  static int avl_delete(ntype **parent, ntype *node) {
    if ((*parent) == node) {
      /* Delete the node */
      /* Reparent the node's children, if any */
      avl_splice(parent);
      return 1;

    } else if (*parent) {
      int d = compare(val(*parent), val(node));
      if ((d <= 0 && avl_delete(&(*parent)->left, node)) ||
          (d >= 0 && avl_delete(&(*parent)->right, node))) {
        avl_update_height(*parent);
        avl_rebalance(parent);
        return 1;
      }
    }

    /* Not found */
    return 0;
  }

  static noinline ntype *avl_find_delete(ntype **parent, vtype value) {
    if (*parent) {
      ntype *node;

      int d = compare(val(*parent), value);
      if (d == 0) {
        node = *parent;
        avl_splice(parent);
        return node;

      } else if ((d < 0 && (node = avl_find_delete(&(*parent)->left, value))) ||
                 (d > 0 && (node = avl_find_delete(&(*parent)->right, value)))) {
        avl_update_height(*parent);
        avl_rebalance(parent);
        return node;
      }
    }

    /* Not found */
    return NULL;
  }

  static noinline ntype *avl_find(ntype *parent, vtype value) {
    if (parent) {
      int d = compare(val(parent), value);
      if (d < 0) {
        return avl_find(parent->left, value);
      } else if (d > 0) {
        return avl_find(parent->right, value);
      } else {
        return parent;
      }
    }
    return NULL;
  }


  inline void avl_print(ntype *tree, int indent) {
    if (!tree) return;
    avl_print(tree->left, indent + 1);
    int j;
    for (j = 0; j < indent; j++) fprintf(stderr, " ");
    fprintf(stderr, "val=%d h=%d\n", tree->value, tree->height);
    avl_print(tree->right, indent + 1);
  }

  inline int r() {
    return rand() / (RAND_MAX / 20000);
  }

  inline void avl_insert_random(ntype **tree, int v) {
    ntype *node = (ntype*)malloc(sizeof(ntype));
    memset(node, 0, sizeof(ntype));
    node->value = v;
    avl_insert(tree, node);
  }

  inline void avl_delete_random(ntype **tree, int v) {
    ntype *node = avl_find_delete(tree, v);
    if (node) {
      free(node);
    }
  }

  inline void avl_delete_random2(ntype **tree, int v) {
    ntype *node = avl_find(*tree, v);
    if (node) {
      avl_delete(tree, node);
      free(node);
    }
  }

  int main() {
    int i, cnt = 100;
    for (i = 0; i < cnt; i++) {
      avl_insert_random(&tree, r());
    }
    avl_delete_random2(&tree, 18467);
    avl_print(tree, 0);
    fprintf(stderr, " -- %d\n", i);
    return 0;
  }

