#pragma once

struct ref_params_s;
struct client_data_s;

namespace viewmodellag {

// Registers cl_modellag* / cl_modelslide* cvars (HUD_Init time).
void init();
// Grabs the engine default_fov pointer (HUD_VidInit time).
void vidInit();
// Per-frame fov feed from HUD_UpdateClientData.
void onUpdateClientData(const struct client_data_s* c);
// First-person V_CalcRefdef body: viewsize weapon-visibility fudge, viewmodel
// lag (PrimeXT-derived) and strafe slide.
void calcRefdef(struct ref_params_s* pparams);

}
