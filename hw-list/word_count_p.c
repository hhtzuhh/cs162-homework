/*
 * Implementation of the word_count interface using Pintos lists and pthreads.
 *
 * You may modify this file, and are expected to modify it.
 */

/*
 * Copyright © 2021 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_lp.c"
#endif

#ifndef PTHREADS
#error "PTHREADS must be #define'd when compiling word_count_lp.c"
#endif

#include "word_count.h"

void init_words(word_count_list_t* wclist) { 
  /* TODO */
  list_init(&wclist->lst);
  pthread_mutex_init(&wclist->lock, NULL);
}

size_t len_words(word_count_list_t* wclist) {
  /* TODO */
  size_t len;
  pthread_mutex_lock(&wclist->lock);
  len = list_size(&wclist->lst);
  pthread_mutex_unlock(&wclist->lock);
  return len;
}
// Helper function without locking for internal use
static word_count_t* find_word_internal(word_count_list_t* wclist, char* word) {
    struct list_elem* e;
    for (e = list_begin(&wclist->lst); e != list_end(&wclist->lst); e = list_next(e)) {
        word_count_t* wc = list_entry(e, word_count_t, elem);
        if (strcmp(wc->word, word) == 0) {
            return wc;
        }
    }
    return NULL;
}

word_count_t* find_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  // it is like word_count_l.c but with mutexes
  pthread_mutex_lock(&wclist->lock);
  word_count_t* wc = find_word_internal(wclist, word);
  pthread_mutex_unlock(&wclist->lock);
  return wc;
}

word_count_t* add_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  pthread_mutex_lock(&wclist->lock);
  word_count_t *existing = find_word_internal(wclist, word);
  if (existing != NULL) {
    existing->count++;
    pthread_mutex_unlock(&wclist->lock);
    return existing;
  }

  // create new word_count_t
  word_count_t *new_node = malloc(sizeof(word_count_t));
  if (new_node == NULL) {
    pthread_mutex_unlock(&wclist->lock);
    return NULL;
  }
  
  new_node->word = strdup(word);
  if (new_node->word == NULL) {
    free(new_node);
    pthread_mutex_unlock(&wclist->lock);
    return NULL;
  }

  new_node->count = 1;
  list_push_back(&wclist->lst, &new_node->elem);
  pthread_mutex_unlock(&wclist->lock);

  return new_node;

}

void fprint_words(word_count_list_t* wclist, FILE* outfile) {
  /* TODO */
  /* Please follow this format: fprintf(<file>, "%i\t%s\n", <count>, <word>); */

  pthread_mutex_lock(&wclist->lock);
  struct list_elem *e;
  for (e = list_begin(&wclist->lst); e != list_end(&wclist->lst); e = list_next(e)) {
    word_count_t* wc = list_entry(e, word_count_t, elem);
    fprintf(outfile, "%i\t%s\n", wc->count, wc->word);
  }
  pthread_mutex_unlock(&wclist->lock);

}
static bool less_list(const struct list_elem* ewc1, const struct list_elem* ewc2, void* aux) {
  /* TODO */
  word_count_t* wc1 = list_entry(ewc1, word_count_t, elem);
  word_count_t* wc2 = list_entry(ewc2, word_count_t, elem);
    // Cast aux to the correct function pointer type
  bool (*compare)(const word_count_t*, const word_count_t*) = (bool (*)(const word_count_t*, const word_count_t*))aux;

  // Use the passed-in comparator
  return compare(wc1, wc2);
}

void wordcount_sort(word_count_list_t* wclist,
                    bool less(const word_count_t*, const word_count_t*)) {
  /* TODO */
  pthread_mutex_lock(&wclist->lock);
  list_sort(&wclist->lst, less_list, less);
  pthread_mutex_unlock(&wclist->lock);

}
