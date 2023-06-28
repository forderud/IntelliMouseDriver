#if !defined(_VFEATURE_H_)
#define _VFEATURE_H_

NTSTATUS
FireflySetFeature(
    IN  PDEVICE_CONTEXT DeviceContext,
    IN  ULONG           Color
    );

#endif // _VFEATURE_H
