#pragma once

struct ref_params_s;

namespace camera {

// Registers cam_idealheight / cam_idealright (HUD_Init time).
void init();
// Third-person V_CalcRefdef body: shifts the view origin along the camera
// right axis and up.
void calcRefdef(struct ref_params_s* pparams);

}
