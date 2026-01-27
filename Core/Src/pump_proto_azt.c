/* Создать новый файл: Core/Src/pump_proto_azt.c */
#include "pump_proto_azt.h"

static void azt_task(void *ctx) { (void)ctx; }
static bool azt_is_idle(void *ctx) { (void)ctx; return true; }
static PumpProtoResult azt_stub(void *ctx, ...) { (void)ctx; return PUMP_PROTO_ERR; }
static bool azt_pop_event(void *ctx, PumpEvent *out) { (void)ctx; (void)out; return false; }

static const PumpProtoVTable azt_vtbl = {
    .task = azt_task,
    .is_idle = azt_is_idle,
    .send_poll_status = (void*)azt_stub,
    .send_preset_volume = (void*)azt_stub,
    .send_preset_money = (void*)azt_stub,
    .send_stop = (void*)azt_stub,
    .send_resume = (void*)azt_stub,
    .send_end = (void*)azt_stub,
    .send_poll_realtime_volume = (void*)azt_stub,
    .send_poll_realtime_money = (void*)azt_stub,
    .send_read_totalizer = (void*)azt_stub,
    .send_read_transaction = (void*)azt_stub,
    .pop_event = azt_pop_event
};

void PumpProtoAZT_Init(PumpProto *out)
{
    if (out == NULL) return;
    out->vt = &azt_vtbl;
    out->ctx = NULL;
}
