/*
 * Word count application with one thread per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>

#include "word_count.h"
#include "word_helpers.h"
// struct to hold arguments for each thread
typedef struct {
  char* filename;
  word_count_list_t* wclist;
} threads_args_t;

// thread function that processes a file
void* process_file(void* args) {
  threads_args_t* data = (threads_args_t*) args;
  FILE* file = fopen(data->filename, "r");
  if (file == NULL) {
    printf("ERROR; could not open file %s\n", data->filename);
    exit(-1);
  }
  count_words(data->wclist, file);
  fclose(file);
  pthread_exit(NULL);//what is this?
}


/*
 * main - handle command line, spawning one thread per file.
 */
int main(int argc, char* argv[]) {
  /* Create the empty data structure. */
  word_count_list_t word_counts;
  init_words(&word_counts);

  if (argc <= 1) {
    /* Process stdin in a single thread. */
    count_words(&word_counts, stdin);
  } else {
    /* TODO */
    int num_threads = argc - 1;
    // array of threads to be created
    //Needed when num_threads is not known at compile time. 
    //otherwise, use pthread_t threads[num_threads]; , C99 seems to support this, at compile time
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    threads_args_t *threads_data = malloc(num_threads * sizeof(threads_args_t));
    // create a thread for each file
    for (int i = 0; i < num_threads; i++) {
      threads_data[i].filename = argv[i + 1];
      threads_data[i].wclist = &word_counts;

      int rc = pthread_create(&threads[i], NULL, process_file, &threads_data[i]);
      if (rc) {
        printf("ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
      }
    }
    // wait for all threads to finish
    for (int i = 0; i < num_threads; i++) {
      pthread_join(threads[i], NULL);
    }
    //clean up
    free(threads);
    free(threads_data);
  }

  /* Output final result of all threads' work. */
  wordcount_sort(&word_counts, less_count);
  fprint_words(&word_counts, stdout);
  return 0;
}
