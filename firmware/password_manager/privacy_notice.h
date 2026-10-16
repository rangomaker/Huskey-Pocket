#pragma once
// Aviso de la implementacion / Implementation notice.
// Cada implementador debe sustituir AMBOS textos por su propio aviso antes
// de distribuir dispositivos o firmware. Ver docs/PRIVACY.es.md y PRIVACY.md.
// Cambiar la version al modificar el aviso. No provoca una nueva lectura
// en dispositivos ya configurados: el asistente solo corre al primer uso.
#define HP_PRIVACY_NOTICE_VERSION "template-1"

static const char HP_PRIVACY_TEXT_ES[] =
    "AVISO PENDIENTE DE CONFIGURAR\n\n"
    "Esta copia contiene el texto de referencia del repositorio Huskey Pocket, "
    "no el aviso de privacidad de una implementacion concreta.\n\n"
    "Quien adapte, implemente o distribuya este proyecto debe redactar, "
    "publicar y mantener su propio aviso de privacidad y, cuando corresponda, "
    "sus propios terminos de uso. Debe identificar al responsable y su "
    "contacto, y describir el tratamiento de datos de su implementacion.\n\n"
    "Los autores del repositorio no asumen esa gestion por quienes lo "
    "implementen. La licencia del codigo se encuentra en LICENSE.\n\n"
    "Solicita el aviso aplicable a quien te proporciono este dispositivo "
    "o firmware. Continuar solo confirma la lectura de este mensaje; "
    "no sustituye un aviso propio ni constituye consentimiento para "
    "un tratamiento de datos no descrito.";

static const char HP_PRIVACY_TEXT_EN[] =
    "NOTICE NOT CONFIGURED\n\n"
    "This copy contains reference text from the Huskey Pocket repository, "
    "not the privacy notice for a specific implementation.\n\n"
    "Anyone adapting, implementing or distributing this project must write, "
    "publish and maintain their own privacy notice and, where appropriate, "
    "their own terms of use. They must identify the responsible party and "
    "their contact details, and describe how their implementation handles data.\n\n"
    "The repository authors do not manage this on behalf of implementers. "
    "The code license is available in LICENSE.\n\n"
    "Request the applicable notice from whoever supplied this device or "
    "firmware. Continuing only acknowledges that you have read this message; "
    "it does not replace an implementation-specific notice or constitute "
    "consent to data processing that has not been described.";

static const char HP_PRIVACY_ACK_ES[] = "He leido este aviso";
static const char HP_PRIVACY_ACK_EN[] = "I have read this notice";
static const char HP_PRIVACY_FOOTER_ES[] =
    "El implementador es responsable de proporcionar y mantener su aviso.";
static const char HP_PRIVACY_FOOTER_EN[] =
    "The implementer is responsible for providing and maintaining their notice.";
