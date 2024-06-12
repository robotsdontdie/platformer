#pragma once
#include <windows.h>

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <wchar.h>
#include <math.h>

#include <d2d1.h>
#include <d2d1_3.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>

#include "resource.h"

template<class Interface>
inline void SafeRelease(Interface** ppInterfaceToRelease)
{
    if (*ppInterfaceToRelease != NULL)
    {
        (*ppInterfaceToRelease)->Release();
        (*ppInterfaceToRelease) = NULL;
    }
}

#ifndef Assert
#if defined( DEBUG ) || defined( _DEBUG )
#define Assert(b) do {if (!(b)) {OutputDebugStringA("Assert: " #b "\n";}} while(0)
#else
#define Assert(b)
#endif // DEBUG || _DEBUG
#endif

#ifndef HIST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

#define RETURN_IF_FAILED(hr) do{if (!SUCCEEDED(hr)) return hr;} while(false)

#define SCREEN_WIDTH 200
#define SCREEN_HEIGHT 150

#define PLAYER_WIDTH 10
#define PLAYER_HEIGHT 20

#define ENEMY_WIDTH 10
#define ENEMY_HEIGHT 10

#define NUM_GEO 20
#define NUM_ENEMIES 5
#define NUM_TEXTURES 20

const float gravity = 550.f;

struct GameState;

struct Input
{
    enum Type
    {
        NONE,
        LEFT_DOWN,
        LEFT_UP,
        RIGHT_DOWN,
        RIGHT_UP,
        UP_DOWN,
        UP_UP,
        DOWN_DOWN,
        DOWN_UP,
        JUMP_DOWN,
        JUMP_UP,
        SPECIAL_DOWN,
        SPECIAL_UP,
    };
};


struct MovementDirection
{
    enum Type
    {
        NONE = 0x0,
        LEFT = 0x1,
        RIGHT = 0x2,
        UP = 0x4,
        DOWN = 0x8,

    };
};

struct Action
{
    enum Type
    {
        NONE       = 0x0,
        MOVE_LEFT  = 0x1,
        MOVE_RIGHT = 0x2,
        JUMP       = 0x4,
    };
};

class Geo
{
public:
    enum BlockType
    {
        BLOCK_NONE       = 0x0,
        BLOCK_BREAKABLE  = 0x1,
        BLOCK_COIN       = 0x2,
        BLOCK_TYPE_COUNT = 0x3,
    };

    enum GameplayState
    {
        GAMEPLAY_NONE,
        HAS_COIN,
        EMPTY,
    };

    enum AnimState
    {
        ANIM_NONE,
        CYCLE_QUESTION,
        BUMPED,
    };

    D2D1_RECT_F GetRectF() const
    {
        return D2D1::RectF(left, top, right, bottom);
    }

    D2D1_RECT_F GetRenderRectF() const
    {
        return D2D1::RectF(left, top + animYOffset, right, bottom + animYOffset);
    }

    void Initialize(float left, float top, float right, float bottom, int textureId, int type)
    {
        active = true;
        this->left = left;
        this->top = top;
        this->right = right;
        this->bottom = bottom;
        this->textureId = textureId;

        this->type = type >= BLOCK_TYPE_COUNT
            ? BlockType::BLOCK_NONE
            : static_cast<Geo::BlockType>(type);

        gameplayState = GAMEPLAY_NONE;
        animState = ANIM_NONE;
        animYOffset = 0.f;
        spriteOffset = 0.f;

        if (type == BLOCK_COIN)
        {
            gameplayState = HAS_COIN;
            animState = CYCLE_QUESTION;
        }
    }

    void Bump()
    {
        if (type == BLOCK_COIN && gameplayState == HAS_COIN)
        {
            gameplayState = EMPTY;
            animState = BUMPED;
            animTime = 0.f;
            spriteOffset = 30.f;
        }
    }

    void TickAnim(float delta)
    {
        const static float bumpTime = 0.2f;
        const static float bumpSize = -5.f;
        const static float questionCycleRate = 2.5f;

        animTime += delta;

        switch (animState)
        {
        case BUMPED:
            if (animTime < bumpTime / 2.f)
            {
                animYOffset = animTime * (bumpSize / (bumpTime / 2.f));
            }
            else if (animTime < bumpTime)
            {
                animYOffset = (bumpTime - animTime) * (bumpSize / (bumpTime / 2.f));
            }
            else
            {
                animState = ANIM_NONE;
                animYOffset = 0.f;
            }
            break;
        case CYCLE_QUESTION:
            switch (static_cast<int>(animTime * questionCycleRate) % 3)
            {
            case 1:
                spriteOffset = 10.f;
                break;
            case 2:
                spriteOffset = 20.f;
                break;
            case 0:
                spriteOffset = 0.f;
            default:
                break;
            }
        default:
            break;
        }
    }

    bool active;
    float left;
    float top;
    float right;
    float bottom;
    int textureId;
    BlockType type;
    GameplayState gameplayState;
    AnimState animState;
    float animTime;
    float animYOffset;
    float spriteOffset;
};

class Actor
{
public:

    void Initialize(float x, float y, float width, float height, float runSpeed)
    {
        this->x = x;
        this->y = y;
        this->width = width;
        this->height = height;
        yVel = 0;
        falling = false;
        action = Action::NONE;
        this->runSpeed = runSpeed;
        animTime = 0.f;
        spriteOffset = 0.f;
        spriteFlip = false;
    }

    D2D1_RECT_F GetRectF() const
    {
        return D2D1::RectF(x, y, x + width, y + height);
    }

    D2D1_RECT_F GetFallingRectF() const
    {
        return D2D1::RectF(x, y + 0.1f, x + width, y + height + 0.1f);
    }

    bool ResolveGeoCollisions(GameState &gameState, MovementDirection::Type actorMovement);

    MovementDirection::Type UpdateMovement(float delta);

    void CheckFalling(const GameState &gameState);

    void TickAnim(float delta, float nominalOffset, float runOffset1, float runOffset2, float fallingOffset)
    {
        animTime += delta;
        while (animTime > 100.f)
        {
            animTime -= 100.f;
        }
        const static float runAnimRate = 6.f;

        if (falling)
        {
            spriteOffset = fallingOffset;
        }
        else if (action & (Action::MOVE_LEFT | Action::MOVE_RIGHT))
        {
            if (static_cast<int>(animTime * runAnimRate) % 2)
            {
                spriteOffset = runOffset1;
            }
            else
            {
                spriteOffset = runOffset2;
            }
        }
        else
        {
            spriteOffset = nominalOffset;
        }

        if (action & (Action::MOVE_LEFT | Action::MOVE_RIGHT))
        {
            spriteFlip = static_cast<bool>(action & Action::MOVE_LEFT);
        }

        if (spriteFlip)
        {
            spriteOffset = -spriteOffset - 10.f;
        }
    }

    float x;
    float y;
    float width;
    float height;
    float yVel;
    bool falling;
    Action::Type action;
    float runSpeed;
    float animTime;
    float spriteOffset;
    bool spriteFlip;
};

class Player
{
public:
    void Reset();

    void HandleInput(Input::Type &input);

    bool ResolveCollisions(GameState &gameState, MovementDirection::Type actorMovement);

    void TickSimulation(GameState &gameState, float delta);

    void TickAnim(float delta)
    {
        actor.TickAnim(delta, 0.f, 10.f, 20.f, 30.f);
    }

    bool isDead;
    Actor actor;
};

struct LevelEntity
{
    enum Type
    {
        END = 0x0,
        GEO = 0x1,
        ENEMY = 0x2,
        TEXTURE = 0x3,
    };
};

struct LevelCursor
{
    short* next;
};

class Enemy
{
public:
    enum Type
    {
        CAT = 0x1,
    };

    void Initialize(float x, float y, Type type, int textureId)
    {
        active = true;
        isDead = false;
        this->type = type;
        actor.Initialize(x, y, ENEMY_WIDTH, ENEMY_HEIGHT, 45);
        actor.action = Action::MOVE_LEFT;
        this->textureId = textureId;
    }

    void ResolveCollisions(GameState& gameState, MovementDirection::Type actorMovement);
    void TickSimulation(GameState& gameState, float delta);

    void TickAnim(float delta)
    {
        actor.TickAnim(delta, 0.f, 0.f, 10.f, 0.f);
    }

    bool active;
    bool isDead;
    Type type;
    Actor actor;
    int textureId;
};

class GlobalAnimation
{
public:
    enum Type
    {
        NONE,
        DEATH,
    };

    bool active;
    Type type;
    float elapsed;

    void Activate(Type type);

    void Tick(GameState& gameState, float delta);

private:
    void TickDeath(GameState& gameState, float delta);
};

struct GameState
{
    bool needsReset;
    int levelId;
    LevelCursor level;
    Input::Type input;
    Player player;
    Geo geo[NUM_GEO];
    Enemy enemies[NUM_ENEMIES];
    GlobalAnimation anim;
    float cameraScroll;
    float frameRate;
    float simTime;
};

class Platformer
{
public:
    Platformer();
    ~Platformer();

    // Register the windows class and call methods for instantiating drawing resources.
    HRESULT Initialize();

    // Process and dispatch messages;
    void RunMessageLoop();

    bool ProcessInput(UINT message, WPARAM wParam, LPARAM lParam);

private:
    // Initialize device-independent resources.
    HRESULT CreateDeviceIndependentResources();

    // Initialize device-dependent resources.
    HRESULT CreateDeviceResources();

    // Release device-dependent resources.
    void DiscardDeviceResources();

    void ResetGame();

    bool LoadLevelGeo(short*& next)
    {
        float peekLeft = static_cast<float>(*(next + 1));
        if (peekLeft > m_gameState.cameraScroll + SCREEN_WIDTH)
        {
            return true;
        }

        next++; // type
        float left = static_cast<float>(*next++);
        float top = static_cast<float>(*next++);
        float right = left + static_cast<float>(*next++);
        float bottom = top + static_cast<float>(*next++);
        int textureId = static_cast<int>(*next++);
        int type = static_cast<int>(*next++);
        AllocateGeo(left, top, right, bottom, textureId, type);

        return false;
    }

    bool LoadLevelEnemy(short*& next)
    {
        float peekLeft = static_cast<float>(*(next + 1));
        if (peekLeft > m_gameState.cameraScroll + SCREEN_WIDTH)
        {
            return true;
        }

        next++; // type
        float left = static_cast<float>(*next++);
        float top = static_cast<float>(*next++);
        float type = static_cast<float>(*next++);
        int textureId = static_cast<int>(*next++);
        AllocateEnemy(left, top, (Enemy::Type)type, textureId);

        return false;
    }

    bool LoadResourceImage(int resourceId, ID2D1Bitmap** pBitmap)
    {
        HRSRC hRes = FindResource(
            HINST_THISCOMPONENT,
            MAKEINTRESOURCE(resourceId),
            L"Image");
        if (hRes == NULL)
        {
            return false;
        }

        HGLOBAL hResLoad = LoadResource(HINST_THISCOMPONENT, hRes);
        if (hResLoad == NULL)
        {
            return false;
        }

        void* pImageData = LockResource(hResLoad);
        if (pImageData == NULL)
        {
            return false;
        }

        DWORD imageDataSize = SizeofResource(HINST_THISCOMPONENT, hRes);
        if (imageDataSize == 0)
        {
            return false;
        }

        IWICStream* pStream = NULL;
        HRESULT hr = m_pIWICFactory->CreateStream(&pStream);

        if (SUCCEEDED(hr))
        {
            hr = pStream->InitializeFromMemory(reinterpret_cast<BYTE*>(pImageData), imageDataSize);
        }
        
        IWICBitmapDecoder* pDecoder = NULL;
        if (SUCCEEDED(hr))
        {
            hr = m_pIWICFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);
        }

        IWICBitmapFrameDecode* pSource = NULL;
        if (SUCCEEDED(hr))
        {
            hr = pDecoder->GetFrame(0, &pSource);
            if (SUCCEEDED(hr))
            {
                UINT blah;
                pSource->GetSize(&blah, &blah);
            }
        }

        IWICFormatConverter* pConverter = NULL;
        if (SUCCEEDED(hr))
        {
            hr = m_pIWICFactory->CreateFormatConverter(&pConverter);
        }

        if (SUCCEEDED(hr))
        {
            hr = pConverter->Initialize(
                pSource,
                GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone,
                NULL,
                0.f,
                WICBitmapPaletteTypeMedianCut);
        }

        if (SUCCEEDED(hr))
        {
            hr = m_pRenderTarget->CreateBitmapFromWicBitmap(pConverter, NULL, pBitmap);
        }

        SafeRelease(&pStream);
        SafeRelease(&pDecoder);
        SafeRelease(&pSource);
        SafeRelease(&pConverter);

        return SUCCEEDED(hr);
    }

    void LoadLevelTexture(short*& next)
    {
        next++; // type
        int levelTextureId = static_cast<int>(*next++);
        int resourceTextureId = static_cast<int>(*next++);

        if (levelTextureId < 0 || levelTextureId >= NUM_TEXTURES)
        {
            return;
        }

        SafeRelease(&m_pTextureBitmaps[levelTextureId]);
        LoadResourceImage(resourceTextureId, &m_pTextureBitmaps[levelTextureId]);
    }

    void LoadLevelEntities()
    {
        short* &next = m_gameState.level.next;
        bool done = false;
        while (!done)
        {
            int peek = *next;
            
            switch (peek)
            {
            case LevelEntity::GEO:
                done = LoadLevelGeo(next);
                break;
            case LevelEntity::ENEMY:
                done = LoadLevelEnemy(next);
                break;
            case LevelEntity::TEXTURE:
                LoadLevelTexture(next);
                break;
            case LevelEntity::END:
            default:
                done = true;
                break;
            }
        }
    }

    bool LoadLevel()
    {
        HRSRC hRes = FindResource(
            HINST_THISCOMPONENT,
            MAKEINTRESOURCE(TO_LEVEL_RES(m_gameState.levelId)),
            LEVEL_RES_NAME);
        if (hRes == NULL)
        {
            return false;
        }

        HGLOBAL hResLoad = LoadResource(HINST_THISCOMPONENT, hRes);
        if (hResLoad == NULL)
        {
            return false;
        }

        LPVOID hResLock = LockResource(hResLoad);
        if (hResLock == NULL)
        {
            return false;
        }

        m_gameState.level.next = static_cast<short*>(hResLock);
        LoadLevelEntities();
        return true;
    }

    int AllocateGeo(float left, float top, float right, float bottom, int textureId, int type)
    {
        for (int index = 0; index < NUM_GEO; index++)
        {
            Geo& geo = m_gameState.geo[index];
            if (geo.active)
            {
                continue;
            }

            geo.Initialize(left, top, right, bottom, textureId, type);

            return index;
        }

        return -1;
    }

    void DeallocateGeo(int index)
    {
        m_gameState.geo[index].active = false;
    }

    void DeallocateAllGeo()
    {
        for (int index = 0; index < NUM_GEO; index++)
        {
            DeallocateGeo(index);
        }
    }

    int AllocateEnemy(float x, float y, Enemy::Type type, int textureId)
    {
        for (int index = 0; index < NUM_ENEMIES; index++)
        {
            Enemy& enemy = m_gameState.enemies[index];

            if (enemy.active)
            {
                continue;
            }

            enemy.Initialize(x, y, type, textureId);

            HRESULT hr = m_pDirect2dFactory->CreateRectangleGeometry(
                D2D1::RectF(0, 0, enemy.actor.width, enemy.actor.height),
                &(m_pEnemyRects[index]));

            if (!SUCCEEDED(hr))
            {
                return -1;
            }

            return index;
        }

        return -1;
    }

    void DeallocateEnemy(int index)
    {
        m_gameState.enemies[index].active = false;
        SafeRelease(&(m_pEnemyRects[index]));
    }

    void DeallocateAllEnemies()
    {
        for (int index = 0; index < NUM_ENEMIES; index++)
        {
            DeallocateEnemy(index);
        }
    }

    float GetTimeDelta();
    bool PumpMessages();
    void TickSimulation(float delta);
    bool TickGame();

    // Draw content.
    HRESULT RenderGame();

    // Resize the render target;
    void OnResize(UINT width, UINT height);

    // The windows procedure.
    static LRESULT CALLBACK WndProc(
        HWND hWnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

private:
    // Application
    HWND m_hwnd;
    LARGE_INTEGER m_lastFrameTime;
    LARGE_INTEGER m_performanceFrequency;
    GameState m_gameState;

    // WIC
    IWICImagingFactory* m_pIWICFactory;
    
    // Base
    ID2D1Factory* m_pDirect2dFactory;
    ID2D1HwndRenderTarget* m_pRenderTarget;

    // Text
    IDWriteFactory* m_pWriteFactory;
    IDWriteTextFormat* m_pDebugTextFormat;

    // Geometry
    ID2D1RectangleGeometry* m_pEnemyRects[NUM_ENEMIES];
    
    // Brushes
    ID2D1SolidColorBrush* m_pLightSlateGrayBrush;
    ID2D1SolidColorBrush* m_pDarkRedBrush;
    ID2D1SolidColorBrush* m_pCornflowerBlueBrush;
    ID2D1SolidColorBrush* m_pInnerSquareBrush;
    
    // Textures
    ID2D1Bitmap* m_pTextureBitmaps[NUM_TEXTURES];
    ID2D1BitmapBrush* m_pTextureBrushes[NUM_TEXTURES];
};