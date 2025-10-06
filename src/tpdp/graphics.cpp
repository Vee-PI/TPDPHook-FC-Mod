#include "graphics.h"
#include "../hook.h"

int MakeGraph(int SizeX, int SizeY, int NotUse3DFlag)
{
    return RVA(0x1d1a60).ptr<int(__cdecl*)(int, int, int)>()(SizeX, SizeY, NotUse3DFlag);
}

int DeleteSharingGraph(int GrHandle)
{
    return RVA(0x1c5620).ptr<int(__cdecl*)(int)>()(GrHandle);
}

int LoadGraph(const char *FileName, int NotUse3DFlag)
{
    return RVA(0x1d5e90).ptr<int(__cdecl*)(const char*, int)>()(FileName, NotUse3DFlag);
}

int LoadDivGraph(const char *FileName, int AllNum, int XNum, int YNum, int XSize, int YSize, int *HandleBuf, int NotUse3DFlag)
{
    return RVA(0x1d61e0).ptr<int(__cdecl*)(const char*, int, int, int, int, int, int*, int)>()(FileName, AllNum, XNum, YNum, XSize, YSize, HandleBuf, NotUse3DFlag);
}

int DrawGraph(int x, int y, int GrHandle, int TransFlag)
{
    return RVA(0x1cc3c0).ptr<int(__cdecl*)(int, int, int, int)>()(x, y, GrHandle, TransFlag);
}

int DrawExtendGraph(int x1, int y1, int x2, int y2, int GrHandle, int TransFlag)
{
    return RVA(0x1cca80).ptr<int(__cdecl*)(int, int, int, int, int, int)>()(x1, y1, x2, y2, GrHandle, TransFlag);
}

int DrawRotaGraph(int x, int y, double ExRate, double Angle, int GrHandle, int TransFlag, int TurnFlag)
{
    return RVA(0x1cd270).ptr<int(__cdecl*)(int, int, double, double, int, int, int)>()(x, y, ExRate, Angle, GrHandle, TransFlag, TurnFlag);
}

int DrawGraphF(float xf, float yf, int GrHandle, int TransFlag)
{
    return RVA(0x1cc720).ptr<int(__cdecl*)(float, float, int, int)>()(xf, yf, GrHandle, TransFlag);
}

int DrawExtendGraphF(float x1f, float y1f, float x2f, float y2, int GrHandle, int TransFlag)
{
    return RVA(0x1cce60).ptr<int(__cdecl*)(float, float, float, float, int, int)>()(x1f, y1f, x2f, y2, GrHandle, TransFlag);
}

int DrawRotaGraphF(float xf, float yf, double ExRate, double Angle, int GrHandle, int TransFlag, int TurnFlag)
{
    return RVA(0x1cd890).ptr<int(__cdecl*)(float, float, double, double, int, int, int)>()(xf, yf, ExRate, Angle, GrHandle, TransFlag, TurnFlag);
}
