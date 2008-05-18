/******************************************************************************************/
/*                                                                                        */
/*  WinTris - Tetris For Windows							  */
/*  Original: August 17, 2002                                                             */
/*  Modified: May 18, 2008								  */
/*  Dave Behnke                                                                           */
/*                                                                                        */
/******************************************************************************************/

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <mmsystem.h>
#include "resource.h"

HINSTANCE g_hInstance = NULL;
HWND g_hWnd = NULL;

HBITMAP hbmBuffer = NULL, hbmBackground = NULL;
HDC hdcBuffer = NULL, hdcBackground = NULL;

DWORD current_time = 0, last_time = 0;
BOOL fDropped = FALSE, fStart = FALSE, fActive = FALSE;

PTCHAR szBuffer;
const int STRING_BUFFER_SIZE = 256;

const int COLOR_COUNT = 9;

enum color_type { RED = 0, ORANGE, YELLOW, GREEN, BLUE, WHITE, MAGENTA, BLACK, GRAY };

COLORREF color_value[COLOR_COUNT] = { RGB( 255, 0, 0 ), RGB( 255, 128, 0 ), RGB( 255, 255, 0 ), 
									RGB( 0, 255, 0 ), RGB( 0, 0, 255 ), RGB( 255, 255, 255 ), RGB( 255, 0, 128 ), 									 
									RGB( 0, 0, 0 ), RGB( 127, 127, 127) };

HBRUSH brush_index[COLOR_COUNT] = { 0 };

int text_offset_x = 0, text_offset_y = 0;
int level = 19, rows_per_level = 0, full_rows = 0, total_rows = 0, score = 0;
unsigned long speed[20] = { 300, 295, 290, 285, 280, 275, 250, 225, 200, 175, 
							170, 165, 160, 150, 145, 140, 135, 130, 125, 100 };

const int PIECE_COUNT = 7;

const int INFO_WIDTH = 6;

const int FIELD_WIDTH = 12;
const int FIELD_HEIGHT = 28;

const int BRICK_WIDTH = 16;
const int BRICK_HEIGHT = 16;

const int SCORE_MAX_NAME = 8;

struct score_t
{
	TCHAR name[SCORE_MAX_NAME];
	int score;
};

struct score_t hall_of_fame[3];

struct shape_t
{
	int count;
	int shape[4];
	enum color_type color;
};

struct shape_t shapes[7] = 
{	  										  
	{ 0x0001, 0xCC00, 0x0000, 0x0000, 0x0000, RED },
	{ 0x0004, 0x4444, 0x0F00, 0x2222, 0x0F00, ORANGE },
	{ 0x0004, 0x4E00, 0x4C40, 0x0E40, 0x4640, YELLOW },
	{ 0x0004, 0x4460, 0x0E80, 0xC440, 0x2E00, GREEN },
	{ 0x0004, 0x44C0, 0x8E00, 0x6440, 0x0E20, BLUE },
	{ 0x0002, 0x4C80, 0xC600, 0x0000, 0x0000, WHITE },
	{ 0x0002, 0x8C40, 0x6C00, 0x0000, 0x0000, MAGENTA }
};

struct piece_t
{
	int x, y;
	int rotation;
	int shape;
};

struct piece_t active_piece, next_piece;

struct brick_t
{
	RECT rect;
	enum color_type color;
};

struct brick_t field[FIELD_HEIGHT][FIELD_WIDTH + INFO_WIDTH];

void create_piece(struct piece_t *active_piece, struct piece_t *next_piece)
{
	active_piece->rotation = next_piece->rotation;
	active_piece->shape = next_piece->shape;
	active_piece->x = 4;
	active_piece->y = 0;

	next_piece->shape = rand() % 7;
	next_piece->rotation = rand() % shapes[next_piece->shape].count;
	next_piece->x = 13;
	next_piece->y = 3;
}

// return TRUE if move is possible, FALSE if impossible
inline BOOL check_piece(struct piece_t *piece)
{
	int row = 0, col = 0;
	
	for (int bit = 0x8000; bit >= 0x0001; bit >>= 1)
	{
		if (shapes[piece->shape].shape[piece->rotation] & bit) {
			if (field[piece->y + row][piece->x + col].color != BLACK) {
				return FALSE;		
			}
		}

		col++;
		if (col == 4) {
			row++;
			col = 0;
		}
	}

	return TRUE;
}

void rotate_piece(struct piece_t *piece)
{
	int previous_rotation = piece->rotation;

	piece->rotation++;
	if (piece->rotation == shapes[piece->shape].count)
		piece->rotation = 0;

	if (!check_piece(piece))
		piece->rotation = previous_rotation;
}

void erase_piece(struct piece_t *piece)
{
	int col = 0, row = 0;
	
	for (int bit = 0x8000; bit >= 0x0001; bit >>= 1)
	{
		if (shapes[piece->shape].shape[piece->rotation] & bit) {
			field[piece->y + row][piece->x + col].color = BLACK;
		}

		col++;
		if (col == 4) {
			row++;
			col = 0;
		}
	}
}

void draw_piece(struct piece_t *piece)
{
	int col = 0, row = 0;
	
	for (int bit = 0x8000; bit >= 0x0001; bit >>= 1)
	{
		if (shapes[piece->shape].shape[piece->rotation] & bit) {
			field[piece->y + row][piece->x + col].color = shapes[piece->shape].color;
		}

		col++;
		if (col == 4) {
			row++;
			col = 0;
		}
	}
}

void left_piece(struct piece_t *piece)
{
	--piece->x;

	if (!check_piece(piece))
		++piece->x;
}

void right_piece(struct piece_t *piece)
{
	++piece->x;

	if (!check_piece(piece))
		--piece->x;
}

// false if piece not moved down - true if piece moved down
BOOL down_piece(struct piece_t *piece)
{
	++piece->y;
	if (!check_piece(piece)) {
		--piece->y;
		return FALSE;
	}

	return TRUE;
}

int drop_piece(struct piece_t *piece)
{
	int height = piece->y;

	do {
		++piece->y;
	} while (check_piece(piece));
	--piece->y;

	return piece->y - height;
}

int next_full_row(void)
{
	int count;

	for (int row = FIELD_HEIGHT - 2; row > 0; row--)
	{
		count = 0;

		for (int col = 1; col < FIELD_WIDTH - 1; col++)
		{
			if (field[row][col].color != BLACK)
				count++;
		}

		if (count == FIELD_WIDTH - 2)
			return row;
	}

	return -1;
}

void remove_row(int row)
{
	for (int j = row; j > 0; j--)
	{
		for (int k = 0; k < FIELD_WIDTH; k++)
		{
			field[j][k].color = field[j - 1][k].color;
		}
	}
}

int remove_full_rows(void)
{
	int count = 0;
	int row = -1;
	
	while ((row = next_full_row()) != -1)
	{
		remove_row(row);
		count++;
	}

	return count;
}

void make_field(int x, int y)
{
	for (int row = 0; row < FIELD_HEIGHT; row++)
	{
		for (int col = 0; col < FIELD_WIDTH + INFO_WIDTH; col++)
		{
			if (col == 0 || col >= FIELD_WIDTH - 1 || row == FIELD_HEIGHT - 1)
				field[row][col].color = GRAY;
			else
				field[row][col].color = BLACK;

			SetRect(&field[row][col].rect, x + (BRICK_WIDTH * col) + 1, y + (BRICK_HEIGHT * row) + 1, 
					x + (BRICK_WIDTH * col + BRICK_WIDTH) - 1, y + (BRICK_HEIGHT * row + BRICK_HEIGHT) - 1);
		}
	}

	for (int row = 2; row < 7; row++)
	{
		for (int col = 12; col < 17; col++)
		{
			field[row][col].color = BLACK;
		}
	}
}

void clear_field(enum color_type color)
{
	for (int row = 0; row < FIELD_HEIGHT - 1; row++)
	{
		for (int col = 1; col < FIELD_WIDTH - 1; col++)
		{
			field[row][col].color = color;
		}
	}
}

void draw_field(HDC hdc)
{
	for (int row = 0; row < FIELD_HEIGHT - 1; row++)
	{
		for (int col = 1; col < FIELD_WIDTH - 1; col++)
		{
			FillRect(hdc, &field[row][col].rect, brush_index[field[row][col].color]);
		}
	}

	for (int row = 2; row < 7; row++)
	{
		for (int col = 12; col < 17; col++)
		{
			FillRect(hdc, &field[row][col].rect, brush_index[field[row][col].color]);
		}
	}
}

void render_frame(HWND hWnd)
{
	BitBlt(hdcBuffer, 0, 0, BRICK_WIDTH * (FIELD_WIDTH + INFO_WIDTH - 1), BRICK_HEIGHT * (FIELD_HEIGHT - 1), hdcBackground, 0, 0, SRCCOPY);
	
	draw_field(hdcBuffer);
	
	ZeroMemory(szBuffer, sizeof(TCHAR) * STRING_BUFFER_SIZE);
	_stprintf_s(szBuffer, STRING_BUFFER_SIZE, TEXT("%0.6d"), level + 1);
	TextOut(hdcBuffer, BRICK_WIDTH * (FIELD_WIDTH - 1) + text_offset_x, BRICK_HEIGHT * 9 + text_offset_y, szBuffer, static_cast<int>(_tcslen(szBuffer)));

	ZeroMemory(szBuffer, sizeof(TCHAR) * STRING_BUFFER_SIZE);
	_stprintf_s(szBuffer, STRING_BUFFER_SIZE, TEXT("%0.6d"), total_rows);
	TextOut(hdcBuffer, BRICK_WIDTH * (FIELD_WIDTH - 1) + text_offset_x, BRICK_HEIGHT * 13 + text_offset_y, szBuffer, static_cast<int>(_tcslen(szBuffer)));

	ZeroMemory(szBuffer, sizeof(TCHAR) * STRING_BUFFER_SIZE);
	_stprintf_s(szBuffer, STRING_BUFFER_SIZE, TEXT("%0.6d"), score);
	TextOut(hdcBuffer, BRICK_WIDTH * (FIELD_WIDTH - 1) + text_offset_x, BRICK_HEIGHT * 17 + text_offset_y, szBuffer, static_cast<int>(_tcslen(szBuffer)));

	HDC hdc = GetDC(hWnd);
	BitBlt(hdc, 0, 0, BRICK_WIDTH * (FIELD_WIDTH + INFO_WIDTH - 1), BRICK_HEIGHT * (FIELD_HEIGHT - 1), hdcBuffer, 0, 0, SRCCOPY);	
	ReleaseDC(hWnd, hdc);
}

void process_input(void)
{
	static SHORT LastKeyPressed = 0;
	static DWORD StartTime = 0;

	if (LastKeyPressed)
	{
		if (GetAsyncKeyState(LastKeyPressed) & 0x8000)
		{
			if ((timeGetTime() - StartTime) >= 175)
			{
				LastKeyPressed = 0;
			}
			else
			{
				return;
			}
		}
		else
		{
			LastKeyPressed = 0;
		}
	}

	if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && !fStart)
	{
		fStart = !fStart;

		clear_field(BLACK);

		level = 0, rows_per_level = 0, full_rows = 0, total_rows = 0, score = 0;

		draw_piece(&active_piece);
		draw_piece(&next_piece);

		LastKeyPressed = VK_SPACE;
	}
	else if ((GetAsyncKeyState(VK_UP) & 0x8000) && fStart)
	{
		erase_piece(&active_piece);
		rotate_piece(&active_piece);					
		draw_piece(&active_piece);

		LastKeyPressed = VK_UP;
	}
	else if ((GetAsyncKeyState(VK_RIGHT) & 0x8000) && fStart)
	{
		erase_piece(&active_piece);
		right_piece(&active_piece);
		draw_piece(&active_piece);

		LastKeyPressed = VK_RIGHT;
	}
	else if ((GetAsyncKeyState(VK_LEFT) & 0x8000) && fStart)
	{
		erase_piece(&active_piece);
		left_piece(&active_piece);
		draw_piece(&active_piece);

		LastKeyPressed = VK_LEFT;
	}
	else if ((GetAsyncKeyState(VK_DOWN) & 0x8000) && fStart)
	{
		erase_piece(&active_piece);
		score += drop_piece(&active_piece);
		draw_piece(&active_piece);

		LastKeyPressed = VK_DOWN;
	}
	else if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
	{
		PostMessage(g_hWnd, WM_CLOSE, 0, 0);
	}

	if (LastKeyPressed)
	{
		StartTime = timeGetTime();
	}
}

unsigned long hash_time(void)
{
	unsigned long hash = 0, now = GetTickCount();
	char *p = (char *)&now;
	
	for (int i = 0; i < sizeof(unsigned long); i++)
		hash = hash * (2U * CHAR_MAX) + *p++;

	return hash;
}

void read_hof(void)
{
	HKEY hKey;
	DWORD dwDisp;

	RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Tetris"), 0, TEXT(""), 0, KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);

	if (dwDisp == REG_CREATED_NEW_KEY)
	{
		ZeroMemory(&hall_of_fame, sizeof(struct score_t) * 3);
			
		TCHAR szText[] = TEXT(".......");
		for (int i = 0; i < 3; i++)
		{
			_tcscpy_s(hall_of_fame[i].name, SCORE_MAX_NAME, szText);				
			hall_of_fame[i].score = 0;
		}
	
		RegSetValueEx(hKey, NULL, 0, REG_BINARY, (PBYTE)&hall_of_fame, sizeof(struct score_t) * 3);
	}
	else
	{
		DWORD size = sizeof(struct score_t) * 3;
		RegQueryValueEx(hKey, NULL, NULL, NULL, (PBYTE)&hall_of_fame, &size);
	}

	RegCloseKey(hKey);
}

void write_hof(void)
{
	HKEY hKey;
	DWORD dwDisp;

	RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Tetris"), 0, TEXT(""), 0, KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);

	if (dwDisp == REG_CREATED_NEW_KEY)
	{
		ZeroMemory(&hall_of_fame, sizeof(struct score_t) * 3);
			
		TCHAR szText[] = TEXT(".......");
		for (int i = 0; i < 3; i++)
		{
			_tcscpy_s(hall_of_fame[i].name, SCORE_MAX_NAME, szText);				
			hall_of_fame[i].score = 0;
		}
	}

	RegSetValueEx(hKey, NULL, 0, REG_BINARY, (PBYTE)&hall_of_fame, sizeof(struct score_t) * 3);
	RegCloseKey(hKey);
}

BOOL CALLBACK HOFDlgProc(HWND hWndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    switch (message) 
    { 
	case WM_INITDIALOG:
		{
			SetClassLong(hWndDlg, GCL_HICON, (LONG) LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_TETRIS)));

			SetDlgItemText(hWndDlg, IDC_HOFNAME1, hall_of_fame[0].name);
			SetDlgItemText(hWndDlg, IDC_HOFNAME2, hall_of_fame[1].name);
			SetDlgItemText(hWndDlg, IDC_HOFNAME3, hall_of_fame[2].name);
	
			ZeroMemory(szBuffer, STRING_BUFFER_SIZE);
			_stprintf_s(szBuffer, STRING_BUFFER_SIZE, TEXT("%d"), hall_of_fame[0].score);
			SetDlgItemText(hWndDlg, IDC_HOFSCORE1, szBuffer);

			ZeroMemory(szBuffer, STRING_BUFFER_SIZE);
			_stprintf_s(szBuffer, STRING_BUFFER_SIZE, TEXT("%d"), hall_of_fame[1].score);
			SetDlgItemText(hWndDlg, IDC_HOFSCORE2, szBuffer);

			ZeroMemory(szBuffer, STRING_BUFFER_SIZE);
			_stprintf_s(szBuffer, STRING_BUFFER_SIZE, TEXT("%d"), hall_of_fame[2].score);
			SetDlgItemText(hWndDlg, IDC_HOFSCORE3, szBuffer);
			
			return TRUE;
		}
    case WM_COMMAND:
		{
			switch (LOWORD(wParam)) 
			{ 
				case IDOK:
					{
						EndDialog(hWndDlg, wParam); 
						return TRUE;
					}
			} 
		}
	} 

    return FALSE; 
} 

BOOL CALLBACK OkDlgProc(HWND hWndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    switch (message) 
    {
		case WM_INITDIALOG:
		{
			SetClassLong(hWndDlg, GCL_HICON, (LONG) LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_TETRIS)));
			return TRUE;
		}
        case WM_COMMAND:
			{
				switch (LOWORD(wParam)) 
				{ 
					case IDOK:
						{
							EndDialog(hWndDlg, wParam); 
							return TRUE;
						}
				} 
			}
	} 

    return FALSE; 
} 

BOOL validate_name(PTCHAR szBuffer)
{
	BOOL result = FALSE;

	if (!szBuffer) 
		return result;

	size_t idx = 0;
	size_t len = _tcslen(szBuffer);
		
	while (idx < len)
	{
		if (!_istspace(szBuffer[idx]))
		{
			result = TRUE;
			break;			
		}

		idx++;
	}

	return result;
}

BOOL CALLBACK NameDlgProc(HWND hWndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    switch (message) 
    { 
	case WM_INITDIALOG:
		{
			SetClassLong(hWndDlg, GCL_HICON, (LONG) LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_TETRIS)));

			if (GetDlgCtrlID((HWND) wParam) != IDC_NAME) 
			{ 
				SetFocus(GetDlgItem(hWndDlg, IDC_NAME)); 
				return FALSE; 
			}
			
			return TRUE;
		}
    case WM_COMMAND:
		{
			switch (LOWORD(wParam)) 
			{ 
				case IDOK:
					{
						ZeroMemory(szBuffer, STRING_BUFFER_SIZE);
						GetDlgItemText(hWndDlg, IDC_NAME, szBuffer, SCORE_MAX_NAME);

						if (!validate_name(szBuffer))
						{
							MessageBox(NULL, TEXT("Please enter your name."), TEXT("Tetris"), MB_OK);
							return FALSE;
						}

						if (score > hall_of_fame[0].score)
						{
							hall_of_fame[2] = hall_of_fame[1];
							hall_of_fame[1] = hall_of_fame[0];
							_tcscpy_s(hall_of_fame[0].name, SCORE_MAX_NAME, szBuffer);
							hall_of_fame[0].score = score;
						}
						else if (score > hall_of_fame[1].score)
						{
							hall_of_fame[2] = hall_of_fame[1];
							_tcscpy_s(hall_of_fame[1].name, SCORE_MAX_NAME, szBuffer);
							hall_of_fame[1].score = score;
						}
						else if (score > hall_of_fame[2].score)
						{
							_tcscpy_s(hall_of_fame[2].name, SCORE_MAX_NAME, szBuffer);
							hall_of_fame[2].score = score;
						}

						write_hof();
						EndDialog(hWndDlg, wParam); 
						return TRUE;
					}
			} 
		}
	} 

    return FALSE; 
} 

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static TIMECAPS tc;
	static HFONT hFont;
	static HGDIOBJ hPrevObject;

	switch (Msg)
	{
	case WM_ACTIVATEAPP:
		{
			fActive = (BOOL) wParam;
			return 0L;
		}
	case WM_KEYDOWN:
		{
			switch (wParam)
			{
			case VK_F1:
				{
					DialogBox(g_hInstance, MAKEINTRESOURCE(DLG_HELP), g_hWnd, (DLGPROC)OkDlgProc);
					break;
				}
			case VK_F2:
				{
					DialogBox(g_hInstance, MAKEINTRESOURCE(DLG_HOF), g_hWnd, (DLGPROC)HOFDlgProc);
					break;
				}
			case VK_F3:
				{
					DialogBox(g_hInstance, MAKEINTRESOURCE(DLG_ABOUT), g_hWnd, (DLGPROC)OkDlgProc);
					break;
				}
			}
			return 0L;
		}
	case WM_CREATE:
		{			
			szBuffer = new TCHAR[STRING_BUFFER_SIZE];
			if (!szBuffer)
			{				
				return -1;
			}

			read_hof();

			timeGetDevCaps(&tc, sizeof(TIMECAPS));
			timeBeginPeriod(tc.wPeriodMin);
			
			srand(hash_time());

			for (int i = 0; i < COLOR_COUNT; i++)
			{
				brush_index[i] = CreateSolidBrush(color_value[i]);
			}

			make_field(-BRICK_WIDTH, 0);

			// call twice to prime piece creation
			create_piece(&active_piece, &next_piece);
			create_piece(&active_piece, &next_piece);

			// create double buffer
			{
				HDC hdc = GetDC(hWnd);

				hdcBuffer = CreateCompatibleDC(hdc);			
				hbmBuffer = CreateCompatibleBitmap(hdc, BRICK_WIDTH * (FIELD_WIDTH + INFO_WIDTH - 1), BRICK_HEIGHT * (FIELD_HEIGHT - 1));
				SelectObject(hdcBuffer, hbmBuffer);

				hdcBackground = CreateCompatibleDC(hdc);
				hbmBackground = CreateCompatibleBitmap(hdc, BRICK_WIDTH * (FIELD_WIDTH + INFO_WIDTH - 1), BRICK_HEIGHT * (FIELD_HEIGHT - 1));
				SelectObject(hdcBackground, hbmBackground);

				ReleaseDC(hWnd, hdc);
			}

			// basic layout
			{
				RECT r;
				SetRect(&r, 0, 0, BRICK_WIDTH * (FIELD_WIDTH - 2), BRICK_HEIGHT * (FIELD_HEIGHT - 1));
				FillRect(hdcBackground, &r, (HBRUSH) GetStockObject(BLACK_BRUSH));
				
				SetRect(&r, BRICK_WIDTH * (FIELD_WIDTH - 2), 0, BRICK_WIDTH * (FIELD_WIDTH + INFO_WIDTH - 1), BRICK_HEIGHT * (FIELD_HEIGHT - 1));
				FillRect(hdcBackground, &r, (HBRUSH) GetStockObject(LTGRAY_BRUSH));
				
				DrawEdge(hdcBackground, &r, EDGE_BUMP, BF_RECT);

				hFont = CreateFont(-MulDiv(14, GetDeviceCaps(hdcBackground, LOGPIXELSY), 72), 0, 0, 0, 700, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, TEXT("Comic Sans MS"));
				hPrevObject = SelectObject(hdcBackground, hFont);

				SetBkMode(hdcBackground, TRANSPARENT);
				SetTextColor(hdcBackground, RGB(255, 0, 0));
			}

			// next
			{
				RECT r;
				SetRect(&r, BRICK_WIDTH * (FIELD_WIDTH - 1), BRICK_HEIGHT * 2, BRICK_WIDTH * (FIELD_WIDTH + 4), BRICK_HEIGHT * (7));
				FillRect(hdcBackground, &r, (HBRUSH) GetStockObject(BLACK_BRUSH));
				
				InflateRect(&r, 2, 2);

				DrawEdge(hdcBackground, &r, EDGE_BUMP, BF_RECT);

				SIZE s;
				TCHAR szText[] = TEXT("Next");
				
				GetTextExtentPoint32(hdcBackground, szText, ARRAYSIZE(szText) - 1, &s);
				TextOut(hdcBackground, (r.left + (r.right - r.left) / 2) - (s.cx / 2), r.top - s.cy, szText, static_cast<int>(_tcslen(szText)));
			}

			// level
			{
				RECT r;
				SetRect(&r, BRICK_WIDTH * (FIELD_WIDTH - 1), BRICK_HEIGHT * 9, BRICK_WIDTH * (FIELD_WIDTH + 4), BRICK_HEIGHT * (11));
				FillRect(hdcBackground, &r, (HBRUSH) GetStockObject(BLACK_BRUSH));
				
				InflateRect(&r, 2, 2);

				DrawEdge(hdcBackground, &r, EDGE_BUMP, BF_RECT);

				SIZE s;
				TCHAR szText[] = TEXT("Level");
				
				GetTextExtentPoint32(hdcBackground, szText, ARRAYSIZE(szText) - 1, &s);
				TextOut(hdcBackground, (r.left + (r.right - r.left) / 2) - (s.cx / 2), r.top - s.cy, szText, static_cast<int>(_tcslen(szText)));
			}
			
			// lines
			{
				RECT r;
				SetRect(&r, BRICK_WIDTH * (FIELD_WIDTH - 1), BRICK_HEIGHT * 13, BRICK_WIDTH * (FIELD_WIDTH + 4), BRICK_HEIGHT * (15));
				FillRect(hdcBackground, &r, (HBRUSH) GetStockObject(BLACK_BRUSH));
				
				InflateRect(&r, 2, 2);

				DrawEdge(hdcBackground, &r, EDGE_BUMP, BF_RECT);

				SIZE s;
				TCHAR szText[] = TEXT("Lines");
				
				GetTextExtentPoint32(hdcBackground, szText, ARRAYSIZE(szText) - 1, &s);
				TextOut(hdcBackground, (r.left + (r.right - r.left) / 2) - (s.cx / 2), r.top - s.cy, szText, static_cast<int>(_tcslen(szText)));
			}
		
			// score
			{
				RECT r;
				SetRect(&r, BRICK_WIDTH * (FIELD_WIDTH - 1), BRICK_HEIGHT * 17, BRICK_WIDTH * (FIELD_WIDTH + 4), BRICK_HEIGHT * (19));
				FillRect(hdcBackground, &r, (HBRUSH) GetStockObject(BLACK_BRUSH));
				
				InflateRect(&r, 2, 2);

				DrawEdge(hdcBackground, &r, EDGE_BUMP, BF_RECT);

				SIZE s;
				TCHAR szText[] = TEXT("Score");
				
				GetTextExtentPoint32(hdcBackground, szText, ARRAYSIZE(szText) - 1, &s);
				TextOut(hdcBackground, (r.left + (r.right - r.left) / 2) - (s.cx / 2), r.top - s.cy, szText, static_cast<int>(_tcslen(szText)));
			}

			DeleteObject(hFont);
			SelectObject(hdcBackground, hPrevObject);

			hFont = CreateFont(-MulDiv(8, GetDeviceCaps(hdcBackground, LOGPIXELSY), 72), 0, 0, 0, 700, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, TEXT("Comic Sans MS"));
			hPrevObject = SelectObject(hdcBackground, hFont);

			{
				RECT r;
				SetRect(&r, BRICK_WIDTH * (FIELD_WIDTH - 2), BRICK_HEIGHT * 20 + 4, BRICK_WIDTH * (FIELD_WIDTH + 5), BRICK_HEIGHT * (28));

				TCHAR szText[] = TEXT("F1 - Help\nF2 - Hall of Fame\nF3 - About\nSpace - Start\nESC - Exit");
				SetTextColor(hdcBackground, RGB(0, 0, 255));
				DrawText(hdcBackground, szText, ARRAYSIZE(szText) - 1, &r, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
			}

			DeleteObject(hFont);
			SelectObject(hdcBackground, hPrevObject);

			hFont = CreateFont(-MulDiv(14, GetDeviceCaps(hdcBackground, LOGPIXELSY), 72), 0, 0, 0, 700, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, TEXT("Comic Sans MS"));
			hPrevObject = SelectObject(hdcBuffer, hFont);

			{
				TCHAR szText[7] = TEXT("000000");
				SIZE s;
				GetTextExtentPoint32(hdcBuffer, szText, ARRAYSIZE(szText) - 1, &s);
				text_offset_x = ((((BRICK_WIDTH * (FIELD_WIDTH + 4)) - (BRICK_WIDTH * (FIELD_WIDTH - 1))) - s.cx) / 2);
				text_offset_y = (((BRICK_HEIGHT * 2) - s.cy) / 2);
			}
			

			SetBkMode(hdcBuffer, TRANSPARENT);
			SetTextColor(hdcBuffer, RGB(255, 255, 255));

			return 0;
		}
	case WM_CLOSE:
		{
			if (MessageBox(hWnd, TEXT("Exit?"), TEXT("Tetris"), MB_YESNO) == IDYES)
				DestroyWindow(hWnd);
			
			return 0;
		}
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			render_frame(hWnd);
			EndPaint(hWnd, &ps);
			return 0L;
		}
	case WM_DESTROY:
		{
			delete szBuffer;

			timeEndPeriod(tc.wPeriodMin);

			for (int i = 0; i < COLOR_COUNT; i++)
			{
				DeleteObject(brush_index[i]);
			}

			DeleteObject(hFont);
			SelectObject(hdcBackground, hPrevObject);
			
			DeleteObject(hbmBackground);
			DeleteDC(hdcBackground);

			DeleteObject(hbmBuffer);
			DeleteDC(hdcBuffer);

			PostQuitMessage(0);
			return 0;
		}
	}
	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	static TCHAR szClassName[] = TEXT("DBEB2C8C-1BCD-409f-985E-276CB19A81B5");
	static TCHAR szAppName[] = TEXT("Tetris");
	WNDCLASSEX wcex;

	HANDLE hMutex = CreateMutex(NULL, TRUE, szClassName);	
	if (ERROR_ALREADY_EXISTS == GetLastError())
	{
		MessageBox(NULL, TEXT("Tetris is already running."), szAppName, MB_OK);
		CloseHandle(hMutex);
		return 0;
	}

	g_hInstance = hInstance;

	wcex.cbClsExtra = 0;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.cbWndExtra = 0;
	wcex.hbrBackground = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TETRIS));
	wcex.hIconSm = NULL;
	wcex.hInstance = hInstance;
	wcex.lpfnWndProc = WndProc;
	wcex.lpszClassName = szClassName;
	wcex.lpszMenuName = NULL;
	wcex.style = 0;

	if (!RegisterClassEx(&wcex))
		return 0;

	DWORD dwStyle = WS_POPUP | WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX | WS_CAPTION;
	DWORD dwExStyle = 0;

	RECT r, w;
	SetRect(&r, 0, 0, BRICK_WIDTH * (FIELD_WIDTH + INFO_WIDTH - 1), BRICK_HEIGHT * (FIELD_HEIGHT - 1));
	AdjustWindowRectEx(&r, dwStyle, FALSE, dwExStyle);
	SystemParametersInfo(SPI_GETWORKAREA, 0, &w, 0);
	int width = r.right - r.left;
	int height = r.bottom - r.top;
	int x = ((w.right - w.left) / 2) - (width / 2);
	int y = ((w.bottom - w.top) / 2) - (height / 2);

	g_hWnd = CreateWindowEx(dwExStyle, szClassName, szAppName, dwStyle, x, y, width, height, NULL, NULL, hInstance, 0);
	if (!g_hWnd)
		return 0;

	ShowWindow(g_hWnd, SW_NORMAL);
	UpdateWindow(g_hWnd);

	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{			
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (fActive)
			{
				process_input();

				current_time = timeGetTime();
				
				if (fStart)
				{
					if ((current_time - last_time) >= speed[level])
					{
						last_time = current_time;

						erase_piece(&active_piece);	
						fDropped = down_piece(&active_piece);
						draw_piece(&active_piece);
						
						if (!fDropped)
						{
							full_rows = remove_full_rows();

							switch (full_rows)
							{
							case 0: break;
							case 1: score += 500; break;
							case 2: score += 1000; break;
							case 3: score += 1500; break;
							case 4: score += 2000; break;
							default: break;
							}
							
							score += ((full_rows * level) + rows_per_level);

							total_rows += full_rows;

							rows_per_level += full_rows;					
							if (rows_per_level > 9)
							{
								rows_per_level = 0;
								if (++level > 19)
								{
									level = 0;
									for (int i = 0; i < 20; i++)
									{
										if ((speed[i] - 10) > 0)
										{
											speed[i] -= 10;
										}
										else
										{
											speed[i] = 0;
										}
									}
								}
							}

							erase_piece(&next_piece);
							
							create_piece(&active_piece, &next_piece);

							if (!check_piece(&active_piece))
							{
								fStart = FALSE;
								clear_field(WHITE);

								if (score > hall_of_fame[2].score)
								{
									DialogBox(g_hInstance, MAKEINTRESOURCE(DLG_NAME), g_hWnd, (DLGPROC)NameDlgProc);
									DialogBox(g_hInstance, MAKEINTRESOURCE(DLG_HOF), g_hWnd, (DLGPROC)HOFDlgProc);
								}

							}
							else
							{
								draw_piece(&active_piece);
								draw_piece(&next_piece);
							}
						}			
					}
				}
				else
				{
					WaitMessage();
				}
			}
		}

		render_frame(g_hWnd);
	}

	ReleaseMutex(hMutex);
	CloseHandle(hMutex);

	return (int) msg.wParam;
}
