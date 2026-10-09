#pragma once

struct ref_params_s;
struct client_data_s;

namespace ViewModelLag {

// Registers cl_modellag* / cl_modelslide* cvars (HUD_Init time).
void Init();
// Grabs the engine default_fov pointer (HUD_VidInit time).
void VidInit();
// Per-frame fov feed from HUD_UpdateClientData.
void OnUpdateClientData(const struct client_data_s* c);
// First-person V_CalcRefdef body: viewsize weapon-visibility fudge, viewmodel
// lag (PrimeXT-derived) and strafe slide.
void CalcRefdef(struct ref_params_s* pparams);

}
