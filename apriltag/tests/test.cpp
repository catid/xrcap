#include <apriltag.h>

int main()
{
    apriltag_detector_t *detector = apriltag_detector_create();
    apriltag_detector_destroy(detector);

    return 0;
}
