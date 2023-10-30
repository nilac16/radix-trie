#pragma once

#ifndef RADIX_TRIE_H
#define RADIX_TRIE_H

#include <stdbool.h>


struct radix_trie {
    struct radix_trie *sib;         /* Next sibling */
    struct radix_trie *child;       /* Child sibling list */
    unsigned           len;         /* Substring length, including nul term if
                                    it is present. It is never safe to index the
                                    substring at this value */
    char               substr[];    /* Contained substring. This is only nul-
                                    terminated if it is a set suffix! */
};


/** @brief Insert @p str into the tree
 *  @param root
 *      Address of pointer to root. This parameter may not be NULL, but it may
 *      and is expected to point to a NULL pointer in certain circumstances
 *  @param str
 *      Nul-terminated string to insert
 *  @returns Nonzero on error
 */
int radix_trie_insert(struct radix_trie **root, const char *str);


/** @brief Look up @p str
 *  @param root
 *      Radix tree root (not const, but not modified). This parameter may be
 *      NULL
 *  @param str
 *      Query string
 *  @returns true if @p str was found
 */
bool radix_trie_lookup(struct radix_trie *root, const char *str);


/** @brief Determine if @p prefix exists in the set without necessarily being an
 *      element thereof
 *  @param root
 *      Radix tree root. This parameter may be NULL
 *  @param prefix
 *      Prefix to match
 *  @returns true if @p prefix was matched
 */
bool radix_trie_match(struct radix_trie *root, const char *prefix);


/** @brief Delete @p str from the tree
 *  @param root
 *      Radix tree root. This may not be NULL, but it may POINT to a NULL
 *      pointer
 *  @param str
 *      String to remove. If this is not found, this function silently returns
 *  @returns Zero if @p str was found, positive if not, and negative if fusing
 *      nodes failed. On failure, @p str is still successfully removed, and the
 *      tree is still usable, but it will suffer from fragmentation until it is
 *      destroyed
 */
int radix_trie_delete(struct radix_trie **root, const char *str);


/** @brief Destroy the tree
 *  @param root
 *      Root pointer, may be NULL
 */
void radix_trie_free(struct radix_trie *root);


/** @brief Callback for the iterator function 
 *  @param str
 *      The fused string, *always* nul-terminated
 *  @param data
 *      Data supplied to the iterator function
 *  @return Nonzero to end iteration immediately
 */
typedef int radix_trie_forfn_t(const char *str, void *data);

/** The default buffer size used by the iterator function */
#define RADIX_FORBUFSZ 1024


/** @brief Apply @p fn to each string in the trie, called in radix-sorted order
 *  @note This iterates the strings in the set, not the nodes in the tree
 *  @todo Allow dynamic buffers somehow (definitely not by default)
 *  @param root
 *      Root pointer. This parameter may be NULL
 *  @param fn
 *      Iterator callback
 *  @param data
 *      Iterator callback data
 *  @param buf
 *      If not NULL, a pointer to a user-supplied buffer of length @p len. This
 *      function by default constructs strings in a large stack buffer before
 *      passing them to the callback. Passing your own overrides this behavior
 *  @param len
 *      If @p buf is not NULL, the size of @p buf. No more than @p len
 *      characters, including the nul term, will be written to @p buf. If @p buf
 *      is NULL this parameter is ignored
 *  @returns Nonzero if it terminated early due to the callback
 */
int radix_trie_foreach(const struct radix_trie *root,
                       radix_trie_forfn_t      *fn,
                       void                    *data,
                       char                    *buf,
                       size_t                   len);


#endif /* RADIX_TRIE_H */
