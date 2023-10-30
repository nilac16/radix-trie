#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "radix.h"


/** @brief Compare @p str to the substring in node, and find the first index
 *      where it differs
 *  @param node
 *      Node to check
 *  @param str
 *      String to check
 *  @returns The index of the first character that differs between @p node and
 *      @p str, the index of @p str's nul term, or the length of @p node's
 *      substring if they were equal up to that point, whichever occurs first.
 *      The result can always safely index @p str
 *  @note In general, if the characters indexed by this result in both strings
 *      are equal, then @p node is nul-terminated and you have found a match. Be
 *      sure to check it against the node's substring length before indexing!
 */
static size_t radix_trie_diff(const struct radix_trie *node, const char *str)
{
    size_t res = 0;

    while (res < node->len && node->substr[res] == str[res] && str[res]) {
        res++;
    }
    return res;
}


/** @brief Create a suffix node in-place using nul-terminated string @p suffix
 *  @param pos
 *      Position at which to place the new node
 *  @param suffix
 *      Suffix string, moved to nul term on success
 *  @returns Nonzero on failure
 */
static int radix_trie_make_suffix(struct radix_trie **pos, const char **suffix)
{
    struct radix_trie *node;
    size_t len;

    len = strlen(*suffix);
    node = malloc(sizeof *node * (len + 1));
    if (!node) {
        return 1;
    }
    node->sib = *pos;
    node->child = NULL;
    node->len = len + 1;
    strcpy(node->substr, *suffix);
    *pos = node;
    *suffix = NULL;
    return 0;
}


/** @brief Split the substring at @p node at index @p diff across two nodes
 *  @param node
 *      Node to split
 *  @param diff
 *      Index to split upon
 *  @returns Nonzero on error
 */
static int radix_trie_split(struct radix_trie **node, size_t diff)
{
    struct radix_trie *prefix;
    void *newptr;

    prefix = malloc(sizeof *prefix + diff);
    if (!prefix) {
        return 1;
    }
    prefix->sib = (*node)->sib;
    prefix->len = diff;
    memcpy(prefix->substr, (*node)->substr, diff);
    (*node)->sib = NULL;
    (*node)->len -= diff;
    memmove((*node)->substr, (*node)->substr + diff, (*node)->len);
    newptr = realloc(*node, sizeof **node + (*node)->len);
    if (newptr) {
        *node = newptr;
    }
    prefix->child = *node;
    *node = prefix;
    return 0;
}


/** @brief Compare @p str to the substring in @p node. Split if needed, and
 *      advance the input pointers
 *  @param node
 *      The address of a pointer to a pointer to a node. You know what to do
 *      with this
 *  @param str
 *      Address of the current prefix pointer
 *  @returns Nonzero on failure
 */
static int radix_trie_try_split(struct radix_trie ***node, const char **str)
{
    size_t diff;

    diff = radix_trie_diff(**node, *str);
    if (diff < (**node)->len) {
        /* String prefix not fully matched. Only two reasons for this */
        if ((**node)->substr[diff] == (*str)[diff]) {
            /* Matched a string already in the set */
            *str = NULL;
            return 0;
        }
        if (radix_trie_split(*node, diff)) {
            return 1;
        }
    }
    *node = &(**node)->child;
    *str += diff;
    return 0;
}


/** @brief Find @p chr in the sibling list at @p head
 *  @param head
 *      Head of the sibling list
 *  @param chr
 *      Query character
 *  @param[out] len
 *      Number of nodes passed over to get to the return value. The parameter
 *      may NOT be NULL
 *  @returns The node containing @p chr or where it should go
 */
static struct radix_trie **radix_trie_lsfind(struct radix_trie **head, char chr)
{
    while (*head && (*head)->substr[0] < chr) {
        head = &(*head)->sib;
    }
    return head;
}


int radix_trie_insert(struct radix_trie **root, const char *str)
{
    int res = 0;

    do {
        root = radix_trie_lsfind(root, *str);
        if (!*root || (*root)->substr[0] > *str) {
            res = radix_trie_make_suffix(root, &str);
        } else {
            res = radix_trie_try_split(&root, &str);
        }
    } while (str && !res);
    return res;
}


bool radix_trie_lookup(struct radix_trie *root, const char *str)
{
    struct radix_trie fake = {
        .child = root
    };
    size_t diff = 0;

    root = &fake;
    do {
        str += diff;
        root = root->child;
        root = *radix_trie_lsfind(&root, *str);
        if (!root) {
            return false;
        }
        diff = radix_trie_diff(root, str);
    } while (diff == root->len);
    /* String prefix not fully matched */
    return root->substr[diff] == str[diff];
}


bool radix_trie_match(struct radix_trie *root, const char *prefix)
{
    size_t diff;

    do {
        root = *radix_trie_lsfind(&root, *prefix);
        if (!root) {
            return false;
        }
        diff = radix_trie_diff(root, prefix);
        if (diff < root->len) {
            /* If the prefix ended, this is a full prefix match */
            return prefix[diff] == '\0';
        }
        prefix += diff;
        root = root->child;
    } while (*prefix);
    /* Everything prefix-matches the empty string */
    return true;
}


/** @brief Attempt to fuse @p parent with its single child, if this condition is
 *      met
 *  @param parent
 *      Address of pointer to parent. This will be reallocated if conditions are
 *      met
 *  @returns Zero on success, and negative if reallocating @p parent fails. If
 *      this happens, the tree is still in a valid state
 */
static int radix_trie_fuse(struct radix_trie **parent)
{
    struct radix_trie *next, *child;
    unsigned newlen;

    if (!*parent || !(*parent)->child || (*parent)->child->sib) {
        /* No parent, no children, or more than one child */
        return 0;
    }
    child = (*parent)->child;
    newlen = (*parent)->len + child->len;
    next = realloc(*parent, sizeof **parent + newlen);
    if (!next) {
        return -1;
    }
    next->child = child->child;
    memcpy(next->substr + next->len, child->substr, child->len);
    next->len += child->len;
    free(child);
    *parent = next;
    return 0;
}


int radix_trie_delete(struct radix_trie **root, const char *str)
{
    struct radix_trie *next, **parent = &(struct radix_trie *){ NULL };
    size_t diff;
    int res = 0;

    do {
        root = radix_trie_lsfind(root, *str);
        if (!*root) {
            break;
        }
        diff = radix_trie_diff(*root, str);
        if (diff < (*root)->len) {
            /* String prefix not fully matched */
            if ((*root)->substr[diff] == str[diff]) {
                /* String matched a leaf node */
                next = (*root)->sib;
                free(*root);
                *root = next;
                res = radix_trie_fuse(parent);
            }
            break;
        } else {
            parent = root;
            root = &(*root)->child;
            str += diff;
        }
    } while (1);
    return res;
}


void radix_trie_free(struct radix_trie *root)
{
    if (root) {
        radix_trie_free(root->child);
        radix_trie_free(root->sib);
        free(root);
    }
}


/** Wrap up some arguments to the recursive function */
struct radix_iter {
    radix_trie_forfn_t *fn;     /* Callback for strings */
    void               *data;   /* Closure for callback function */
    char               *buf;    /* Buffer base pointer */
    size_t              len;    /* Buffer length */
    jmp_buf             env;    /* jmp_buf for early termination */
};


/** @brief Find min(x, y) */
static size_t minzu(size_t x, size_t y)
{
    return (x < y) ? x : y;
}


/** @brief Recursive tree iterator
 *  @param root
 *      Current node
 *  @param iter
 *      Iteration context structure
 *  @param i
 *      Current buffer index
 *  @throw longjmp if the callback issues a termination request
 */
static void radix_trie_iterate(const struct radix_trie *root,
                               struct radix_iter       *iter,
                               size_t                   i)
{
    size_t space, len;

    if (root) {
        space = (iter->len > i) ? iter->len - i : 0;
        len = minzu(space, root->len);
        memcpy(&iter->buf[i], root->substr, len);
        if (!root->substr[root->len - 1]) {
            /* This is a leaf node */
            iter->buf[iter->len - 1] = '\0';
            if (iter->fn(iter->buf, iter->data)) {
                longjmp(iter->env, 1);
            }
        }
        radix_trie_iterate(root->child, iter, i + root->len);
        radix_trie_iterate(root->sib, iter, i);
    }
}


int radix_trie_foreach(const struct radix_trie *root,
                       radix_trie_forfn_t      *fn,
                       void                    *data,
                       char                    *buf,
                       size_t                   len)
{
    char sbuf[RADIX_FORBUFSZ];
    struct radix_iter iter = {
        .fn   = fn,
        .data = data,
        .buf  = (buf) ? buf : sbuf,
        .len  = (buf) ? len : sizeof sbuf
    };

    if (setjmp(iter.env)) {
        return 1;
    }
    if (iter.len) {
        radix_trie_iterate(root, &iter, 0);
    }
    return 0;
}
