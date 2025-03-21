#include "attention.h"

#include <cmath>
#include <iostream>


bool face_is_looking_at_us(retinaface_object_t face) {
    auto left_eye = face.ponit[0];
    auto right_eye = face.ponit[1];
    auto interocular_dist_pix = sqrt(pow(left_eye.x - right_eye.x, 2) + pow(left_eye.y - right_eye.y, 2));

    auto face_width = face.box.right - face.box.left;
    auto face_height = face.box.bottom - face.box.top;
    float face_aspect_ratio = ((float) face_height)/((float) face_width);

    float interocular_face_ratio = interocular_dist_pix / face_width;
    /***
     * face_aspect_ratio should be approximately the golden ratio ~ 1.618
     * interocular_face_ratio should be approximately 0.5
     * 
     * If face_aspect_ratio is < 1.5 or > 1.72, the face is not looking at the camera
     * if interocular_face_ratio is < 0.4 or > 0.6, the face is not looking at the camera

    CF: https://pmc.ncbi.nlm.nih.gov/articles/PMC2814183/
    and https://www.researchgate.net/publication/341711316_Autoencoder-based_image_processing_framework_for_object_appearance_modifications
    ***/
//    std::cout << "face_aspect_ratio: " << face_aspect_ratio << "  "
//      << "interocular_face_ratio: " << interocular_face_ratio << std::endl;

   auto is_looking = face_aspect_ratio > 1.2 && face_aspect_ratio < 2.0
        && interocular_face_ratio > 0.3 && interocular_face_ratio < 0.7;
    // if (is_looking) {
    //     std::cout << "Face is looking at us" << std::endl;
    // } else {
    //     std::cout << "Face is not looking at us" << std::endl;
    // }
   
    return is_looking;
}