#pragma once

struct ref_params_s;

namespace Camera {

// Registers cam_idealheight / cam_idealright (HUD_Init time).
void Init();
// Third-person V_CalcRefdef body: shifts the view origin along the camera
// right axis and up.
void CalcRefdef(struct ref_params_s* pparams);

}
