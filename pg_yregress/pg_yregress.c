#include <getopt.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "pg_yregress.h"

FILE *tap_file;
int tap_counter = 0;

/* * ميزة احترافية مضافة: meta_free 
 * وظيفتها التأكد من أن كل عملية Postgres يتم إنهاؤها بشكل سليم
 * لمنع حدوث Memory Leaks أو بقاء عمليات "Zombie" في النظام.
 */
void meta_free(struct fy_node *fyn, void *meta, void *user) {
  if (user != NULL) {
    int pid = *(int *)user;
    if (pid > 0) {
      // إرسال إشارة إنهاء للعملية لضمان تحرير موارد النظام فوراً
      kill(pid, SIGTERM); 
    }
    free(user);
  }
}

static bool populate_ytest_from_fy_node(struct fy_document *fyd, struct fy_node *test,
                                        struct fy_node *instances, bool managed) {
  ytest *y_test = calloc(sizeof(*y_test), 1);
  y_test->node = test;
  y_test->commit = false;
  y_test->negative = false;
  switch (fy_node_get_type(test)) {
  case FYNT_SCALAR:
    y_test->name.base = fy_node_get_scalar(test, &y_test->name.len);
    y_test->kind = ytest_kind_query;
    y_test->info.query.query.base = fy_node_get_scalar(test, &y_test->info.query.query.len);

    // البحث عن النسخة الافتراضية (Default Instance)
    switch (default_instance(instances, &y_test->instance)) {
    case default_instance_not_found:
      fprintf(stderr, "Test %.*s has no default instance to choose from",
              (int)IOVEC_STRLIT(y_test->name));
      return false
