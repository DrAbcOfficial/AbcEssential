// Third-person camera offset.
// Ported from ABCEnchance src/exportfuncs.cpp V_CalcRefdef third-person branch;
// lives in its own module per plan 04 §1 (shares the hook, not the file).

#include "Camera.h"

#include <metahook.h>

#include <ref_params.h>

#include <abc/CvarCmd.h>
#include <abc/Math.h>

#include "../Module.h"

namespace camera {

namespace {

using abc::CMathlib;

cvar_t* CamIdealHeightCvar;
cvar_t* CamIdealRightCvar;

}

void init() {
	CamIdealHeightCvar = abc::CREATE_CVAR("cam_idealheight", "0", abc::FCVAR_VALUE, nullptr);
	CamIdealRightCvar = abc::CREATE_CVAR("cam_idealright", "0", abc::FCVAR_VALUE, nullptr);
}

void calcRefdef(ref_params_t* pparams) {
	vec3_t right;
	CMathlib::AngleVectors(pparams->cl_viewangles, nullptr, right, nullptr);
	CMathlib::VectorMultipiler(right, CamIdealRightCvar->value);
	pparams->vieworg[0] += right[0];
	pparams->vieworg[1] += right[1];
	pparams->vieworg[2] += CamIdealHeightCvar->value + right[2];
}

namespace {
class CameraModule final : public abcess::IFeatureModule {
public:
	CameraModule() { abcess::moduleRegistry().add(this); }
	const char* name() const override { return "camera"; }
	void onInit() override { init(); }
};
const CameraModule RegisterModule;
}

}
