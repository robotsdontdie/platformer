#include "Platformer.h"
#include "resource.h"
#include <string>

int WINAPI WinMain(
    _In_ HINSTANCE /* hInstance */,
    _In_opt_ HINSTANCE /* hPrevInstance */,
    _In_ LPSTR /* lpCmdLine */,
    _In_ int /* nCmdShow */)
{
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    if (SUCCEEDED(CoInitialize(NULL)))
    {
        {
            Platformer platformer;

            if (SUCCEEDED(platformer.Initialize()))
            {
                platformer.RunMessageLoop();
            }
        }
        CoUninitialize();
    }

    return 0;
}

Platformer::Platformer() :
    // Application
    m_hwnd(NULL),
    m_lastFrameTime(),
    m_performanceFrequency(),
    m_gameState(),
    // WIC
    m_pIWICFactory(NULL),
    // Base
    m_pDirect2dFactory(NULL),
    m_pRenderTarget(NULL),
    // Text
    m_pWriteFactory(NULL),
    m_pDebugTextFormat(NULL),
    // Geometry
    m_pEnemyRects(),
    // Brushes
    m_pLightSlateGrayBrush(NULL),
    m_pDarkRedBrush(NULL),
    m_pCornflowerBlueBrush(NULL),
    m_pInnerSquareBrush(NULL)
{
    QueryPerformanceCounter(&m_lastFrameTime);
    QueryPerformanceFrequency(&m_performanceFrequency);
}

Platformer::~Platformer()
{
    // Base
    SafeRelease(&m_pDirect2dFactory);
    SafeRelease(&m_pRenderTarget);

    // Text
    SafeRelease(&m_pWriteFactory);
    SafeRelease(&m_pDebugTextFormat);
    
    // Brushes
    SafeRelease(&m_pLightSlateGrayBrush);
    SafeRelease(&m_pDarkRedBrush);
    SafeRelease(&m_pCornflowerBlueBrush);
    SafeRelease(&m_pInnerSquareBrush);
}

void Platformer::RunMessageLoop()
{
    CreateDeviceResources();

    /*
    HBITMAP hBitmap = LoadBitmap(HINST_THISCOMPONENT, MAKEINTRESOURCE(TO_TEXTURE_RES(1)));
    IWICBitmap* pWICBitmap;
    HRESULT hr = m_pIWICFactory->CreateBitmapFromHBITMAP(hBitmap, NULL, WICBitmapIgnoreAlpha, &pWICBitmap);
    ID2D1Bitmap* pBitmap;
    hr = m_pRenderTarget->CreateBitmapFromWicBitmap(pWICBitmap, &pBitmap);
    */

    ResetGame();
    while (TickGame())
    {
        
        if (m_gameState.needsReset)
        {
            ResetGame();
        }
        
        //Sleep(50);
    }
}

HRESULT Platformer::CreateDeviceIndependentResources()
{
    HRESULT hr = S_OK;

    hr = CoCreateInstance(
        CLSID_WICImagingFactory2,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWICImagingFactory2,
        reinterpret_cast<void**>(&m_pIWICFactory));

    // Base
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pDirect2dFactory);

    hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(m_pWriteFactory),
            reinterpret_cast<IUnknown**>(&m_pWriteFactory));
    RETURN_IF_FAILED(hr);

    // Text
    hr = m_pWriteFactory->CreateTextFormat(
        L"Verdana",
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        20,
        L"",
        &m_pDebugTextFormat);
    RETURN_IF_FAILED(hr);

    hr = m_pDebugTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    RETURN_IF_FAILED(hr);

    hr = m_pDebugTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    RETURN_IF_FAILED(hr);

    return hr;
}

HRESULT Platformer::CreateDeviceResources()
{
    HRESULT hr = S_OK;

    if (!m_pRenderTarget)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(
            rc.right - rc.left,
            rc.bottom - rc.top);

        // Create a Direct2D render target;
        hr = m_pDirect2dFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &m_pRenderTarget);

        if (SUCCEEDED(hr))
        {
            // Create a gray brush.
            hr = m_pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::LightSlateGray),
                &m_pLightSlateGrayBrush);
        }

        RETURN_IF_FAILED(hr);

        if (SUCCEEDED(hr))
        {
            hr = m_pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::DarkRed),
                &m_pDarkRedBrush);
        }

        RETURN_IF_FAILED(hr);

        if (SUCCEEDED(hr))
        {
            hr = m_pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::CornflowerBlue),
                &m_pCornflowerBlueBrush);
        }

        RETURN_IF_FAILED(hr);

        if (SUCCEEDED(hr))
        {
            hr = m_pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::MediumPurple),
                &m_pInnerSquareBrush);
        }

        RETURN_IF_FAILED(hr);
    }

    for (int index = 0; index < NUM_TEXTURES; index++)
    {
        if (!SUCCEEDED(hr) || !m_pRenderTarget)
        {
            break;
        }

        if (m_pTextureBitmaps[index] == NULL)
        {
            continue;
        }

        m_pRenderTarget->CreateBitmapBrush(m_pTextureBitmaps[index], &m_pTextureBrushes[index]);
        m_pTextureBrushes[index]->SetExtendModeX(D2D1_EXTEND_MODE_WRAP);
        m_pTextureBrushes[index]->SetExtendModeY(D2D1_EXTEND_MODE_WRAP);
    }

    return hr;
}

void Platformer::DiscardDeviceResources()
{
    SafeRelease(&m_pRenderTarget);
    SafeRelease(&m_pLightSlateGrayBrush);
    SafeRelease(&m_pCornflowerBlueBrush);
    SafeRelease(&m_pInnerSquareBrush);
    for (int index = 0; index < NUM_TEXTURES; index++)
    {
        SafeRelease(&m_pTextureBrushes[index]);
    }
}

void Platformer::ResetGame()
{
    m_gameState.needsReset = false;

    // input
    m_gameState.input = Input::NONE;

    // player
    m_gameState.player.Reset();

    // geo
    DeallocateAllGeo();

    // enemies
    DeallocateAllEnemies();

    // camera
    m_gameState.cameraScroll = 0;

    // level
    m_gameState.levelId = 1;
    LoadLevel();

    // anim
    m_gameState.anim.active = false;
}

float Platformer::GetTimeDelta()
{
    LARGE_INTEGER newTime;
    QueryPerformanceCounter(&newTime);

    float delta = (float)(newTime.QuadPart - m_lastFrameTime.QuadPart)
        / (float)(m_performanceFrequency.QuadPart);
    m_lastFrameTime = newTime;

    return delta;
}

bool Platformer::PumpMessages()
{
    MSG msg;
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true;
}


bool Intersect(
    const D2D_RECT_F& rect1,
    const D2D_RECT_F& rect2,
    const MovementDirection::Type& movement,
    float& verticalAdjustment,
    float& horizontalAdjustment)
{
    if (rect1.right <= rect2.left
        || rect1.left >= rect2.right
        || rect1.bottom <= rect2.top
        || rect1.top >= rect2.bottom)
    {
        return false;
    }

    bool hasVertical = false;
    bool hasHorizontal = false;

    if (movement & MovementDirection::DOWN)
    {
        verticalAdjustment = rect2.top - rect1.bottom;
        hasVertical = true;
    }
    else if (movement & MovementDirection::UP)
    {
        verticalAdjustment = rect2.bottom - rect1.top;
        hasVertical = true;
    }

    if (movement & MovementDirection::LEFT)
    {
        horizontalAdjustment = rect2.right - rect1.left;
        hasHorizontal = true;
    }
    else if (movement & MovementDirection::RIGHT)
    {
        horizontalAdjustment = rect2.left - rect1.right;
        hasHorizontal = true;
    }

    if (hasVertical && hasHorizontal)
    {
        if (std::abs(horizontalAdjustment) > std::abs(verticalAdjustment))
        {
            horizontalAdjustment = 0;
        }
        else
        {
            verticalAdjustment = 0;
        }
    }

    return true;
}

void Platformer::TickSimulation(float delta)
{
    // sim player
    m_gameState.player.TickSimulation(m_gameState, delta);

    // sim enemies
    for (int index = 0; index < NUM_ENEMIES; index++)
    {
        Enemy& enemy = m_gameState.enemies[index];
        if (!enemy.active)
        {
            continue;
        }

        if (enemy.isDead)
        {
            DeallocateEnemy(index);
            continue;
        }

        enemy.TickSimulation(m_gameState, delta);
    }

    // tick animations
    for (Geo& geo : m_gameState.geo)
    {
        geo.TickAnim(delta);
    }

    // update camera boundary
    const static int cameraScrollOffset = SCREEN_WIDTH / 3 * 2;
    Actor& playerActor = m_gameState.player.actor;
    if (playerActor.x > m_gameState.cameraScroll + cameraScrollOffset)
    {
        m_gameState.cameraScroll = m_gameState.player.actor.x - cameraScrollOffset;
    }

    if (playerActor.x < m_gameState.cameraScroll)
    {
        playerActor.x = m_gameState.cameraScroll;
    }

    // unload geo
    for (int geoIndex = 0; geoIndex < NUM_GEO; geoIndex++)
    {
        Geo& geo = m_gameState.geo[geoIndex];
        if (geo.active && geo.right < m_gameState.cameraScroll)
        {
            DeallocateGeo(geoIndex);
        }
    }

    // load new entities
    LoadLevelEntities();
}

bool Platformer::TickGame()
{
    LARGE_INTEGER startTime;
    QueryPerformanceCounter(&startTime);

    if (!PumpMessages())
    {
        return false;
    }

    m_gameState.player.HandleInput(m_gameState.input);

    float delta = GetTimeDelta();
    m_gameState.frameRate = 1.f / delta;
    
    if (m_gameState.anim.active)
    {
        m_gameState.anim.Tick(m_gameState, delta);
    }
    else
    {
        TickSimulation(delta);
    }

    LARGE_INTEGER endTime;
    QueryPerformanceCounter(&endTime);

    m_gameState.simTime = (float)(endTime.QuadPart - startTime.QuadPart)
        / (float)(m_performanceFrequency.QuadPart);

    RenderGame();

    return true;
}

HRESULT Platformer::RenderGame()
{
    HRESULT hr = S_OK;

    hr = CreateDeviceResources();

    if (SUCCEEDED(hr))
    {
        m_pRenderTarget->BeginDraw();
        m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
        m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

        D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();

        D2D1_MATRIX_3X2_F screenScaleTransform = D2D1::Matrix3x2F::Scale(
            D2D1::SizeF(rtSize.width / SCREEN_WIDTH, rtSize.height / SCREEN_HEIGHT));
        D2D1_MATRIX_3X2_F screenScaleTransformInverse = D2D1::Matrix3x2F::Scale(
            D2D1::SizeF(SCREEN_WIDTH / rtSize.width, SCREEN_HEIGHT / rtSize.height));
        D2D1_MATRIX_3X2_F screenScrollTransform = D2D1::Matrix3x2F::Translation(
            D2D1::SizeF(-m_gameState.cameraScroll, 0)
        );

        D2D1_MATRIX_3X2_F screenTransform = screenScrollTransform * screenScaleTransform;

        m_pRenderTarget->SetTransform(screenTransform);

        // Draw a grid background.
        int width = static_cast<int>(rtSize.width);
        int height = static_cast<int>(rtSize.height);
        int gridStartX = ((int)m_gameState.cameraScroll) / 10 * 10;
        for (int x = 0; x < SCREEN_WIDTH + 10; x += 10)
        {
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(static_cast<FLOAT>(gridStartX + x), 0.f),
                D2D1::Point2F(static_cast<FLOAT>(gridStartX + x), SCREEN_HEIGHT),
                m_pLightSlateGrayBrush,
                0.5f);
        }

        for (int y = 0; y < SCREEN_HEIGHT; y += 10)
        {
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(static_cast<FLOAT>(gridStartX + 0), static_cast<FLOAT>(y)),
                D2D1::Point2F(static_cast<FLOAT>(gridStartX + SCREEN_WIDTH + 10), static_cast<FLOAT>(y)),
                m_pLightSlateGrayBrush,
                0.5f);
        }

        D2D1_MATRIX_3X2_F textureScale = D2D1::Matrix3x2F::Scale(D2D1::SizeF(1.f / 1.6f, 1.f / 1.6f));

        for (const Geo& geo : m_gameState.geo)
        {
            if (geo.active)
            {
                D2D1_RECT_F geoRect = geo.GetRenderRectF();
                m_pTextureBrushes[geo.textureId]->SetTransform(textureScale * D2D1::Matrix3x2F::Translation(D2D1::SizeF(geoRect.left - geo.spriteOffset, geoRect.top)));
                m_pRenderTarget->FillRectangle(geoRect, m_pTextureBrushes[geo.textureId]);
            }
        }

        for (const Enemy& enemy : m_gameState.enemies)
        {
            if (enemy.active)
            {
                const Actor& actor = enemy.actor;
                D2D1_RECT_F enemyRect = actor.GetRectF();
                D2D1_MATRIX_3X2_F flip = actor.spriteFlip
                    ? D2D1::Matrix3x2F::Scale(D2D1::SizeF(-1.f, 1.f))
                    : D2D1::Matrix3x2F::Identity();
                D2D1_MATRIX_3X2_F translation = D2D1::Matrix3x2F::Translation(D2D1::SizeF(enemyRect.left - actor.spriteOffset, enemyRect.top));
                m_pTextureBrushes[enemy.textureId]->SetTransform(textureScale * flip * translation);
                m_pRenderTarget->FillRectangle(enemyRect, m_pTextureBrushes[enemy.textureId]);
            }
        }

        // draw player
        {
            const Actor& actor = m_gameState.player.actor;
            D2D1_RECT_F playerRect = actor.GetRectF();
            D2D1_MATRIX_3X2_F flip = actor.spriteFlip
                ? D2D1::Matrix3x2F::Scale(D2D1::SizeF(-1.f, 1.f))
                : D2D1::Matrix3x2F::Identity();
            D2D1_MATRIX_3X2_F translation = D2D1::Matrix3x2F::Translation(D2D1::SizeF(playerRect.left - actor.spriteOffset, playerRect.top));
            m_pTextureBrushes[0]->SetTransform(textureScale * flip * translation);
            m_pRenderTarget->FillRectangle(playerRect, m_pTextureBrushes[0]);
        }
        
        m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
        static int frame = 1;
        std::wstring frameString = std::to_wstring((int)(m_gameState.frameRate));
        m_pRenderTarget->DrawTextW(
            frameString.c_str(),
            static_cast<UINT32>(frameString.length()),
            m_pDebugTextFormat,
            D2D1::RectF(0, 0, 200, 20),
            m_pLightSlateGrayBrush);

        hr = m_pRenderTarget->EndDraw();
    }

    if (hr == D2DERR_RECREATE_TARGET)
    {
        hr = S_OK;
        DiscardDeviceResources();
    }

    return hr;
}

void Platformer::OnResize(UINT width, UINT height)
{
    if (m_pRenderTarget)
    {
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));
    }
}

LRESULT CALLBACK Platformer::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;

    if (message == WM_CREATE)
    {
        LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
        Platformer* pPlatformer = (Platformer*)pcs->lpCreateParams;

        ::SetWindowLongPtrW(
            hWnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(pPlatformer));

        result = 1;
    }
    else
    {
        Platformer* pPlatformer = reinterpret_cast<Platformer*>(static_cast<LONG_PTR>(
            ::GetWindowLongPtrW(
                hWnd,
                GWLP_USERDATA)));

        bool wasHandled = false;

        if (pPlatformer)
        {
            switch (message)
            {
            case WM_SIZE:
                {
                    UINT width = LOWORD(lParam);
                    UINT height = HIWORD(lParam);
                    pPlatformer->OnResize(width, height);
                }
                result = 0;
                wasHandled = true;
                break;

            case WM_DISPLAYCHANGE:
                {
                    InvalidateRect(hWnd, NULL, false);
                }
                result = 0;
                wasHandled = true;
                break;

            case WM_PAINT:
                {
                    ValidateRect(hWnd, NULL);
                }
                result = 0;
                wasHandled = true;
                break;

            case WM_DESTROY:
                {
                    PostQuitMessage(0);
                }
                result = 1;
                wasHandled = true;
                break;

            case WM_KEYDOWN:
            case WM_KEYUP:
                result = 0;
                wasHandled = pPlatformer->ProcessInput(message, wParam, lParam);
                break;
            }
        }

        if (!wasHandled)
        {
            result = DefWindowProc(hWnd, message, wParam, lParam);
        }
    }

    return result;
}

bool Platformer::ProcessInput(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message != WM_KEYDOWN
        && message != WM_KEYUP)
    {
        return false;
    }

    bool down = message == WM_KEYDOWN;

    WORD flags = HIWORD(lParam);
    if ((flags & KF_REPEAT) && down)
    {
        // we dont care about autorepeat messages
        return false;
    }

    Input::Type input = Input::NONE;
    WORD vkCode = LOWORD(wParam);
    switch (vkCode)
    {
    case 0x41: // A
        input = down ? Input::LEFT_DOWN : Input::LEFT_UP;
        break;
    case 0x44: // D
        input = down ? Input::RIGHT_DOWN : Input::RIGHT_UP;
        break;
    case 0x58: // W
        input = down ? Input::UP_DOWN : Input::UP_UP;
        break;
    case 0x53: // S
        input = down ? Input::DOWN_DOWN : Input::DOWN_UP;
        break;
    case VK_SPACE:
        input = down ? Input::JUMP_DOWN : Input::JUMP_UP;
        break;
    case VK_LSHIFT:
        input = down ? Input::SPECIAL_DOWN : Input::SPECIAL_UP;
        break;
    }

    m_gameState.input = input;

    return input != Input::NONE;
}

HRESULT Platformer::Initialize()
{
    HRESULT hr;

    // Initialize device-independent resource, such as the Direct2D factory.
    hr = CreateDeviceIndependentResources();

    if (SUCCEEDED(hr))
    {
        // Register the window class.
        WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = Platformer::WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = sizeof(LONG_PTR);
        wcex.hInstance = HINST_THISCOMPONENT;
        wcex.hbrBackground = NULL;
        wcex.lpszMenuName = NULL;
        wcex.hCursor = LoadCursor(NULL, IDI_APPLICATION);
        wcex.lpszClassName = L"Platformer";

        RegisterClassEx(&wcex);

        // In terms of using the correct DPI, to create a window at a specific size
        // like this, the procedure is to first create the window hidden. Then we get
        // the actual DPI from the HWND (which will be assigned by whichever monitor
        // the window is created on). Then we use SetWindowPos to resize it to the
        // correct DPI-scaled size, then we use ShowWindow to show it.

        m_hwnd = CreateWindow(
            L"Platformer",
            L"Platformer",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            NULL,
            NULL,
            HINST_THISCOMPONENT,
            this);

        if (m_hwnd)
        {
            // Because the SetWindowPos function takes its size in pixels, we
            // obtain the window's DPI, and use it to scale the window size.
            float dpi = (float)GetDpiForWindow(m_hwnd);

            SetWindowPos(
                m_hwnd,
                NULL,
                NULL,
                NULL,
                static_cast<int>(ceil(800.f * dpi / 96.f)),
                static_cast<int>(ceil(600.f * dpi / 96.f)),
                SWP_NOMOVE);
            ShowWindow(m_hwnd, SW_SHOWNORMAL);
            UpdateWindow(m_hwnd);
        }
    }

    return hr;
}

void Player::Reset()
{
    actor.Initialize(0, 0, PLAYER_WIDTH, PLAYER_HEIGHT, 75);
    isDead = false;
}

inline void Player::HandleInput(Input::Type &input)
{
    if (input == Input::LEFT_DOWN)
    {
        actor.action = static_cast<Action::Type>(actor.action | Action::MOVE_LEFT);
    }

    if (input == Input::LEFT_UP)
    {
        actor.action = static_cast<Action::Type>(actor.action & ~Action::MOVE_LEFT);
    }

    if (input == Input::RIGHT_DOWN)
    {
        actor.action = static_cast<Action::Type>(actor.action | Action::MOVE_RIGHT);
    }

    if (input == Input::RIGHT_UP)
    {
        actor.action = static_cast<Action::Type>(actor.action & ~Action::MOVE_RIGHT);
    }

    if (input == Input::JUMP_DOWN)
    {
        actor.action = static_cast<Action::Type>(actor.action | Action::JUMP);
    }

    if (input == Input::JUMP_UP)
    {
        actor.action = static_cast<Action::Type>(actor.action & ~Action::JUMP);
    }

    input = Input::NONE;
}

bool Player::ResolveCollisions(GameState& gameState, MovementDirection::Type actorMovement)
{
    actor.ResolveGeoCollisions(gameState, actorMovement);

    D2D_RECT_F actorRect = actor.GetRectF();
    bool gotKill = false;
    for (Enemy &enemy : gameState.enemies)
    {
        if (!enemy.active)
        {
            continue;
        }

        float verticalAdjustment = 0;
        float horizonalAdjustment = 0;
        bool intersect = Intersect(
            actorRect,
            enemy.actor.GetRectF(),
            actorMovement,
            verticalAdjustment,
            horizonalAdjustment);

        if (intersect)
        {
            if (horizonalAdjustment != 0)
            {
                isDead = true;
            }

            if (verticalAdjustment < 0)
            {
                enemy.isDead = true;
                gotKill = true;
            }
            else
            {
                isDead = true;
            }
        }
    }

    return gotKill;
}

void Player::TickSimulation(GameState& gameState, float delta)
{
    static const float jumpPower = -150;
    MovementDirection::Type movementDirection = actor.UpdateMovement(delta);

    TickAnim(delta);

    // resolve collisions
    bool gotKill = ResolveCollisions(gameState, movementDirection);

    actor.CheckFalling(gameState);           

    // check jump
    if ((actor.falling == false && actor.action & Action::JUMP)
        || gotKill)
    {
        actor.yVel = jumpPower;
        actor.falling = true;
    }

    // check dead zone
    if (actor.y > SCREEN_HEIGHT)
    {
        isDead = true;
    }


    if (gameState.player.isDead)
    {
        gameState.anim.Activate(GlobalAnimation::Type::DEATH);
    }
}

bool Actor::ResolveGeoCollisions(GameState& gameState, MovementDirection::Type actorMovement)
{
    D2D_RECT_F actorRect = GetRectF();
    bool hadHorizonalAdjustment = false;
    for (Geo& geo : gameState.geo)
    {
        if (!geo.active)
        {
            continue;
        }

        float verticalAdjustment = 0;
        float horizonalAdjustment = 0;
        bool intersect = Intersect(
            actorRect,
            geo.GetRectF(),
            actorMovement,
            verticalAdjustment,
            horizonalAdjustment);

        if (intersect)
        {
            x += horizonalAdjustment;
            y += verticalAdjustment;
            actorRect = GetRectF();

            if (verticalAdjustment < 0)
            {
                falling = false;
                yVel = 0;
            }
            else if (verticalAdjustment > 0)
            {
                yVel = 0;
                geo.Bump();
            }
            else if (horizonalAdjustment != 0)
            {
                hadHorizonalAdjustment = true;
            }
        }
    }

    return hadHorizonalAdjustment;
}

MovementDirection::Type Actor::UpdateMovement(float delta)
{
    // update movement
    int movementDirection = MovementDirection::NONE;
    if (action & Action::MOVE_LEFT)
    {
        if (!(action & Action::MOVE_RIGHT))
        {
            x -= (runSpeed * delta);
            movementDirection = movementDirection | MovementDirection::LEFT;
        }
    }
    else if (action & Action::MOVE_RIGHT)
    {
        x += (runSpeed * delta);
        movementDirection = movementDirection | MovementDirection::RIGHT;
    }

    if (falling)
    {
        float actualGravity = gravity;
        if (action & Action::JUMP)
        {
            actualGravity /= 2.5f;
        }

        yVel += (actualGravity * delta);
    }

    if (yVel != 0)
    {
        movementDirection = movementDirection | (yVel > 0 ? MovementDirection::DOWN : MovementDirection::UP);
    }

    float yPrev = y;
    y += (yVel * delta);

    return (MovementDirection::Type)movementDirection;
}

void Actor::CheckFalling(const GameState& gameState)
{
    if (!falling)
    {
        D2D_RECT_F fallingRect = GetFallingRectF();
        bool supported = false;
        for (const Geo& geo : gameState.geo)
        {
            float verticalAlignment = 0;
            float horizontalAlignment = 0;
            if (Intersect(fallingRect, geo.GetRectF(), MovementDirection::DOWN, verticalAlignment, horizontalAlignment))
            {
                supported = true;
                break;
            }
        }
        if (!supported)
        {
            falling = true;
        }
    }
}

void Enemy::ResolveCollisions(GameState& gameState, MovementDirection::Type actorMovement)
{
    bool hadHorizontalAdjustment = actor.ResolveGeoCollisions(gameState, actorMovement);
    if (hadHorizontalAdjustment)
    {
        if (actor.action & Action::MOVE_LEFT)
        {
            actor.action = Action::MOVE_RIGHT;
        }
        else if (actor.action & Action::MOVE_RIGHT)
        {
            actor.action = Action::MOVE_LEFT;
        }
    }

    D2D_RECT_F actorRect = actor.GetRectF();

    float verticalAdjustment = 0;
    float horizonalAdjustment = 0;
    bool intersect = Intersect(
        actorRect,
        gameState.player.actor.GetRectF(),
        actorMovement,
        verticalAdjustment,
        horizonalAdjustment);

    if (intersect)
    {
        if (horizonalAdjustment != 0)
        {
            gameState.player.isDead = true;
        }
        else if (verticalAdjustment < 0)
        {
            gameState.player.isDead = true;
        }
        else
        {
            isDead = true;
        }
    }
}

void Enemy::TickSimulation(GameState& gameState, float delta)
{
    MovementDirection::Type movementDirection = actor.UpdateMovement(delta);

    // resolve collisions
    ResolveCollisions(gameState, movementDirection);

    TickAnim(delta);

    actor.CheckFalling(gameState);

    // check dead zone
    if (actor.y > SCREEN_HEIGHT)
    {
        isDead = true;
    }
}

void GlobalAnimation::Activate(Type type)
{
    active = true;
    this->type = type;
    elapsed = 0;
}

void GlobalAnimation::Tick(GameState& gameState, float delta)
{
    elapsed += delta;
    switch (type)
    {
    case DEATH:
        TickDeath(gameState, delta);
        break;
    default:
        break;
    }
}

void GlobalAnimation::TickDeath(GameState& gameState, float delta)
{
    if (elapsed < 0.5f)
    {
        gameState.player.actor.yVel = 0;
    }
    else if (elapsed < 1.f)
    {
        gameState.player.actor.yVel -= (gravity / 2.f * delta);
    }
    else if (elapsed < 3.f)
    {
        gameState.player.actor.yVel += (gravity / 2.f * delta);
    }
    else
    {
        gameState.needsReset = true;
    }

    gameState.player.actor.y += gameState.player.actor.yVel * delta;
}
