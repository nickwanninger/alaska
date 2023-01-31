#include <alaska.h>
#include <alaska/internal.h>

#ifdef ALASKA_CLASS_TRACKING
static uint64_t alaska_class_access_counts[256];
void alaska_classify_track(uint8_t object_class) { alaska_class_access_counts[object_class]++; }

void alaska_classify_init(void) {}

void alaska_classify_deinit(void) {
  int do_dump = getenv("ALASKA_DUMP_OBJECT_CLASSES") != NULL;
#ifdef ALASKA_CLASS_TRACKING_ALWAYS_DUMP
  do_dump = 1;
#endif

  if (do_dump) {
    printf("class,accesses\n");

#define __CLASS(name, id) \
  if (alaska_class_access_counts[id] != 0) printf(#name ",%zu\n", alaska_class_access_counts[id]);
#include "../include/classes.inc"
#undef __CLASS
  }
}
#endif

uint8_t alaska_get_classification(void *ptr) {
#ifdef ALASKA_CLASS_TRACKING
  handle_t h;
  h.ptr = ptr;
  if (likely(h.flag != 0)) {
    alaska_mapping_t *ent = (alaska_mapping_t *)(uint64_t)h.handle;
    return ent->object_class;
  }
#endif
	return 0;
}
