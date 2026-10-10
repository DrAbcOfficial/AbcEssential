// Third-person camera offset.
// Ported from ABCEnchance src/exportfuncs.cpp V_CalcRefdef third-person branch;
// lives in its own module per plan 04 §1 (shares the hook, not the file).
#include "Camera.h"

#include <metahook.h>

#include <ref_params.h>

#include <abc/CvarCmd.h>
#include <abc/Math.h>

#include "../Module.h"

namespace {

using abc::CMathlib;

cvar_t* s_pCamIdealHeight;
cvar_t* s_pCamIdealRight;

}

void Camera::Init() {
	s_pCamIdealHeight = abc::CREATE_CVAR("cam_idealheight", "0", abc::FCVAR_VALUE, nullptr);
	s_pCamIdealRight = abc::CREATE_CVAR("cam_idealright", "0", abc::FCVAR_VALUE, nullptr);
}

void Camera::CalcRefdef(ref_params_t* pparams) {
	vec3_t vecRight;
	CMathlib::AngleVectors(pparams->cl_viewangles, nullptr, vecRight, nullptr);
	CMathlib::VectorMultipiler(vecRight, s_pCamIdealRight->value);
	pparams->vieworg[0] += vecRight[0];
	pparams->vieworg[1] += vecRight[1];
	pparams->vieworg[2] += s_pCamIdealHeight->value + vecRight[2];
}

namespace {
class CameraModule final : public abcess::IFeatureModule {
public:
	CameraModule() { abcess::moduleRegistry().add(this); }
	const char* name() const override { return "camera"; }
	void onInit() override { Camera::Init(); }
};
const CameraModule RegisterModule;
}
