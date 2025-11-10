#pragma once
#include "../typedefs.h"

// these facilitate calls to the dxlib graphics API functions static linked into the game exe
// see dxlib source code for more information https://dxlib.xsrv.jp/

// creation/deletion functions
HOOKAPI int MakeGraph(int SizeX, int SizeY, int NotUse3DFlag = 0); // create blank texture
HOOKAPI int DeleteSharingGraph(int GrHandle); // free texture handle along with all of its derivatives

// image loading
HOOKAPI int LoadGraph(const char *FileName, int NotUse3DFlag = 0); // load image file
HOOKAPI int LoadDivGraph(const char *FileName, int AllNum, int XNum, int YNum, int XSize, int YSize, int *HandleBuf, int NotUse3DFlag = 0); // load tiled image file (atlas) and create handles for each of its tiles

// integer draw functions
HOOKAPI int DrawGraph(int x, int y, int GrHandle, int TransFlag); // draw texture, TransFlag = transparency
HOOKAPI int DrawExtendGraph(int x1, int y1, int x2, int y2, int GrHandle, int TransFlag); // draw stretched texture
HOOKAPI int DrawRotaGraph(int x, int y, double ExRate, double Angle, int GrHandle, int TransFlag, int TurnFlag = 0); // draw rotated texture, turn flag = direction of rotation??

// float draw functions
HOOKAPI int DrawGraphF(float xf, float yf, int GrHandle, int TransFlag);
HOOKAPI int DrawExtendGraphF(float x1f, float y1f, float x2f, float y2, int GrHandle, int TransFlag);
HOOKAPI int DrawRotaGraphF(float xf, float yf, double ExRate, double Angle, int GrHandle, int TransFlag, int TurnFlag = 0);

// blend mode
// see DX_BLENDMODE_XXX in DxLib.h
HOOKAPI int SetDrawBlendMode(int BlendMode, int BlendParam);
