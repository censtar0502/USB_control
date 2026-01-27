/* Создать новый файл: Core/Src/pump_proto_sanki.c */
#include "pump_proto_sanki.h"

static void sanki_task(void *ctx) { (void)ctx; }
static bool sanki_is_idle(void *ctx) { (void)ctx; return true; }
static PumpProtoResult sanki_stub(void *ctx, ...) { (void)ctx; return PUMP_PROTO_ERR; }
static bool sanki_pop_event(void *ctx, PumpEvent *out) { (void)ctx; (void)out; return false; }

static const PumpProtoVTable sanki_vtbl = {
    .task = sanki_task,
    .is_idle = sanki_is_idle,
    .send_poll_status = (void*)sanki_stub,
    .send_preset_volume = (void*)sanki_stub,
    .send_preset_money = (void*)sanki_stub,
    .send_stop = (void*)sanki_stub,
    .send_resume = (void*)sanki_stub,
    .send_end = (void*)sanki_stub,
    .send_poll_realtime_volume = (void*)sanki_stub,
    .send_poll_realtime_money = (void*)sanki_stub,
    .send_read_totalizer = (void*)sanki_stub,
    .send_read_transaction = (void*)sanki_stub,
    .pop_event = sanki_pop_event
};

void PumpProtoSanki_Init(PumpProto *out)
{
    if (out == NULL) return;
    out->vt = &sanki_vtbl;
    out->ctx = NULL;
}
