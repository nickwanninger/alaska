#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/personality/anchorage.h>


// classification stuff:
static uint64_t alaska_class_access_counts[256];
void alaska_classify_track(uint8_t object_class) {
	// printf("classify %d\n", object_class);
	alaska_class_access_counts[object_class]++;
}



void alaska_classify(void *ptr, uint8_t c) {
#ifdef ALASKA_CLASS_TRACKING
	if (c == 255) c = 0;
	alaska_mapping_t *m = alaska_lookup(ptr);
	if (m != NULL) {
		m->anchorage.object_class = c;
	}
#endif
}



uint8_t alaska_get_classification(void *ptr) {
#ifdef ALASKA_CLASS_TRACKING
	alaska_mapping_t *m = alaska_lookup(ptr);
	if (m != NULL) {
		return m->anchorage.object_class;
	}
#endif
	return 0;
}





void alaska_personality_init(void) {
	// printf("init\n");
}

void alaska_personality_deinit(void) {
	// printf("deinit\n");
#ifdef ALASKA_CLASS_TRACKING
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
#endif
}


