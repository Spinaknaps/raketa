#ifndef RAKETA_COMMAND_HANDLER_H
#define RAKETA_COMMAND_HANDLER_H

#include "pressure_model.h"
#include "protocol.h"

bool command_handle(
    PressureModel *model,
    const ProtocolFrame *request,
    ProtocolFrame *response
);

#endif