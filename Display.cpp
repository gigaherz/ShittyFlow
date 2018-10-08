#include <Windows.h>
#include <mmsystem.h>

#include <math.h>

#include <list>

#include <assert.h>

// powers of two only!
const DWORD WIDTH = 512;
const DWORD HEIGHT = 512;
DWORD buffer[HEIGHT * 2][WIDTH * 2];

HWND hWnd;
BITMAPINFO bmi;

#define map map1

bool enableSource = false;

float terrain[HEIGHT][WIDTH]; // base value (constant)
float water[HEIGHT][WIDTH]; // current water level
float flow[HEIGHT][WIDTH]; // water level difference for the pass (temp)
float given[HEIGHT][WIDTH]; // water given by each cell

int sourceX = WIDTH / 2;
int sourceY = HEIGHT / 2;

float totalSourced;
float totalDrained;

int lastTickCount = 0;

float avgWater = 0;
float avgFlow = 0;
float nRenders = 0;

const extern unsigned char map[];

float totalHeight(int x, int y)
{
    int ix = x&(WIDTH - 1);
    int iy = y&(HEIGHT - 1);
    return terrain[iy][ix] + water[iy][ix];
}

float waterHeight(int x, int y)
{
    int ix = x&(WIDTH - 1);
    int iy = y&(HEIGHT - 1);
    return water[iy][ix];
}

void addFlowToCell(int x, int y, float fl)
{
    int ix = x&(WIDTH - 1);
    int iy = y&(HEIGHT - 1);
    flow[iy][ix] += fl;
}

VOID Render()
{
    float entry = 0;
    float drain = 0;

    float fwater = 0;
    float fflow = 0;

    // Update the water source
    if (enableSource)
    {
        water[sourceY][sourceX] += 50;
        entry += 50;
    }

    // Cleanup the flow information
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            flow[y][x] = 0;
        }
    }

    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            float cWater = water[y][x];

            if (cWater > 0)
            {
                float cHeight = totalHeight(x, y);
                float cFall = 0;
                float tFall = 0;
                float cAvg = 0;
                float tAvg = 0;

                // For each neighbor of the cell
                for (int ty = -1; ty < 2; ty++)
                {
                    for (int tx = -1; tx < 2; tx++)
                    {
                        float nHeight = cHeight;

                        if (tx | ty)
                            nHeight = totalHeight(x + tx, y + ty);

                        cAvg += nHeight;
                        tAvg += 1;
                    }
                }

                cAvg /= tAvg;

                float fGive = cHeight - cAvg;

                if (fGive > cWater)
                    fGive = cWater;

                float maxGive = sqrt(fGive);

                if (maxGive > cWater)
                    maxGive = cWater;

                for (int ty = -1; ty < 2; ty++)
                {
                    for (int tx = -1; tx < 2; tx++)
                    {
                        if (tx | ty)
                        {
                            float nHeight = totalHeight(x + tx, y + ty);
                            float dHeight = (cHeight - nHeight);

                            if (dHeight > 0)
                            {
                                if ((tx&ty & 1) > 0)
                                    dHeight = dHeight / 1.414213562;

                                cFall += sqrt(dHeight);
                                tFall += 1;
                            }
                        }
                    }
                }

                //cFall /= tFall;

                float cGive = cFall;

                if (cGive > maxGive)
                    cGive = maxGive;

                if (cGive < 0.000001f)
                    cGive = 0;

                //float fAmount = (1- 1/(1+max(0,2*(cHeight - cAvg))));

                //float cAmount = cWater * fAmount;

                given[y][x] = cGive;

                // For each neighbor of the cell
                for (int ty = -1; ty < 2; ty++)
                {
                    for (int tx = -1; tx < 2; tx++)
                    {
                        if (tx | ty)
                        {
                            float nHeight = totalHeight(x + tx, y + ty);
                            float dHeight = (cHeight - nHeight);

                            if (dHeight > 0)
                            {

                                if ((tx&ty & 1) > 0)
                                    dHeight = dHeight / 1.414213562;

                                //float fFall = dHeight;

                                float dWater = sqrt(dHeight * 0.9999) * cGive / cFall;

                                //float dWater = (cAmount * fFall / cFall);

                                addFlowToCell(x + tx, y + ty, dWater);
                                addFlowToCell(x, y, -dWater);
                            }
                        }
                    }
                }
            }
        }
    }

    // Apply flow and calculate terrain erosion
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            float pflow = flow[y][x];

            water[y][x] += pflow;

            fflow += abs(pflow);

            if (water[y][x] < 0) // shouldn't happen.
            {
                drain += -water[y][x];
                water[y][x] = 0;
            }

            if (terrain[y][x] <= 0)
            {
                drain += water[y][x];
                water[y][x] = 0;
                terrain[y][x] = 0;
            }

            fwater += water[y][x];
        }
    }

    // 	float corrective = 1;
    // 
    // 	if(fwater > 0)
    // 	{
    // 		corrective = 0.9 + 0.1 * (totalSourced-totalDrained) / fwater;
    // 	}
    // 
    // 	if(corrective != 0)
    // 	{
    // 		// Apply flow and calculate terrain erosion
    // 		for(int y=0;y<HEIGHT;y++)
    // 		{
    // 			for(int x=0;x<WIDTH;x++)
    // 			{
    // 				water[y][x] = water[y][x] * corrective;
    // 			}
    // 		}
    // 	}

    totalSourced += entry;
    totalDrained += drain;
    avgWater += fwater;
    avgFlow += fflow;
    nRenders += 1;

    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            int R, G, B;

            float shade = (terrain[y][x] - terrain[min(511, y + 1)][max(0, x - 1)]) / terrain[y][x];

            shade = shade + 0.1 * ((water[y][x] - water[min(511, y + 1)][max(0, x - 1)]) / terrain[y][x]);

            float pshade = min(4 * max(0, shade), 1);
            float nshade = max(0, 1 + 2 * min(0, shade));

            float fred = 0;
            float fblue = 0;

            if (water[y][x]>0)
                fred = given[y][x]; // / water[y][x];

            if (terrain[y][x]>0)
                fblue = water[y][x] * 200;

            R = nshade * (fred * 5); //+ (log(1+abs(flow[y][x]/water[y][x]))*100));
            G = nshade * (map[y * 512 + x] * 0.5);
            B = nshade * (fblue);

            R = max(0, min(255, R));
            G = max(0, min(255, G));
            B = max(0, min(255, B));

            int C = (R << 16) | (G << 8) | B;

            buffer[(y * 2 + 0)][(x * 2 + 0)] = C;
            buffer[(y * 2 + 0)][(x * 2 + 1)] = C;
            buffer[(y * 2 + 1)][(x * 2 + 0)] = C;
            buffer[(y * 2 + 1)][(x * 2 + 1)] = C;
        }
    }

    HDC winDC = GetDC(hWnd);

    SetDIBitsToDevice(
        winDC,
        0, 0, WIDTH * 2, HEIGHT * 2,
        0, 0, 0, HEIGHT * 2,
        buffer,
        &bmi,
        DIB_RGB_COLORS
        );

    static char m1[1000];

    if (lastTickCount == 0)
        lastTickCount = GetTickCount();

    float elapsed = (GetTickCount() - lastTickCount);

    if (elapsed >= 200)
    {
        avgWater /= nRenders;
        avgFlow /= nRenders;

        float fps = nRenders * 1000 / elapsed;

        sprintf(m1, "Average Flow: %f; Water in map: %f (Sourced: %f; Drained: %f; Difference: %f; Relative Error: %g); FPS: %f",
            avgFlow,
            avgWater,
            totalSourced,
            totalDrained,
            totalSourced - totalDrained,
            (avgWater - (totalSourced - totalDrained)) / avgWater,
            fps
            );

        SetWindowText(hWnd, m1);

        avgFlow = 0;
        avgWater = 0;
        nRenders = 0;
        lastTickCount = GetTickCount();
    }

    ReleaseDC(hWnd, winDC);
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        //Cleanup();
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        //Cleanup();
        PostQuitMessage(0);
        return 0;

    case WM_LBUTTONDOWN:

        sourceX = (LOWORD(lParam) / 2)&(WIDTH - 1);
        sourceY = 511 - (HIWORD(lParam) / 2)&(HEIGHT - 1);
        enableSource = true;

        return 0;
    case WM_RBUTTONDOWN:

        enableSource = !enableSource;

    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}


int main(int argc, char* argv[])
{
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            terrain[y][x] = map[y * 512 + x];
        }
    }

    // Register the window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
        "Rock", NULL };
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(&wc);

    // Create the application's window
    hWnd = CreateWindow("Rock", "Span Rendering",
        WS_OVERLAPPEDWINDOW, 0, 0, 1000, 1000,
        GetDesktopWindow(), NULL, wc.hInstance, NULL);

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biWidth = WIDTH * 2;
    bmi.bmiHeader.biHeight = HEIGHT * 2;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biClrUsed = 0;
    bmi.bmiHeader.biClrImportant = 0;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biSizeImage = 0;

    // Initialize Direct3D
    if (TRUE) //SUCCEEDED( InitD3D( hWnd ) ) )
    {
        // Create the geometry
        if (TRUE) //SUCCEEDED( InitGeometry() ) )
        {
            // Show the window
            ShowWindow(hWnd, SW_SHOWDEFAULT);
            UpdateWindow(hWnd);

            // Enter the message loop
            MSG msg;
            ZeroMemory(&msg, sizeof(msg));
            while (msg.message != WM_QUIT)
            {
                if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                else
                    Render();
            }
        }
    }

    UnregisterClass("Rock", wc.hInstance);
    return 0;
}
