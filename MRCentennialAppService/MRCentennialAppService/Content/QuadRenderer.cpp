//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "QuadRenderer.h"
#include "Common\DirectXHelper.h"
#include <vector> 

using namespace Concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Input::Spatial;

namespace MRCentennialAppService
{
    // Loads vertex and pixel shaders from files and instantiates the quad geometry.
    QuadRenderer::QuadRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, int width, int height) 
        : m_deviceResources(deviceResources)
        , m_width(width)
        , m_height(height)
        , m_sharedTextureHandle(NULL)
    {
        CreateDeviceDependentResources();
        StartFadeIn();
    }

    // This function uses a SpatialPointerPose to position the world-locked hologram
    // two meters in front of the user's heading.
    void QuadRenderer::PositionHologram(SpatialPointerPose^ pointerPose)
    {
        if (pointerPose != nullptr)
        {
            // Get the gaze direction relative to the given coordinate system.
            const float3 headPosition = pointerPose->Head->Position;
            const float3 headDirection = pointerPose->Head->ForwardDirection;

            // The hologram is positioned two meters along the user's gaze direction.
            constexpr float distanceFromUser = 2.0f; // meters
            const float3 gazeAtTwoMeters = headPosition + (distanceFromUser * headDirection);

            // This will be used as the translation component of the hologram's
            // model transform.
            SetPosition(gazeAtTwoMeters);
        }
    }

    // Called once per frame. Rotates the quad, and calculates and sets the model matrix
    // relative to the position transform indicated by hologramPositionTransform.
    void QuadRenderer::Update(const DX::StepTimer& timer)
    {
        // Seconds elapsed since previous frame.
        const float deltaTime = static_cast<float>(timer.GetElapsedSeconds());
        const float oneOverDeltaTime = 1.f / deltaTime;

        // Create a direction normal from the hologram's position to the origin of person space.
        // This is the z-axis rotation.
        XMVECTOR facingNormal = XMVector3Normalize(-XMLoadFloat3(&m_position));

        // Rotate the x-axis around the y-axis.
        // This is a 90-degree angle from the normal, in the xz-plane.
        // This is the x-axis rotation.
        XMVECTOR xAxisRotation = XMVector3Normalize(XMVectorSet(XMVectorGetZ(facingNormal), 0.f, -XMVectorGetX(facingNormal), 0.f));

        // Create a third normal to satisfy the conditions of a rotation matrix.
        // The cross product  of the other two normals is at a 90-degree angle to
        // both normals. (Normalize the cross product to avoid floating-point math
        // errors.)
        // Note how the cross product will never be a zero-matrix because the two normals
        // are always at a 90-degree angle from one another.
        XMVECTOR yAxisRotation = XMVector3Normalize(XMVector3Cross(facingNormal, xAxisRotation));

        // Construct the 4x4 rotation matrix.

        // Rotate the quad to face the user.
        XMMATRIX rotationMatrix = XMMATRIX(
            xAxisRotation,
            yAxisRotation,
            facingNormal,
            XMVectorSet(0.f, 0.f, 0.f, 1.f)
        );

        // Position the quad.
        const XMMATRIX modelTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));

        // The view and projection matrices are provided by the system; they are associated
        // with holographic cameras, and updated on a per-camera basis.
        // Here, we provide the model transform for the sample hologram. The model transform
        // matrix is transposed to prepare it for the shader.
        XMStoreFloat4x4(&m_modelConstantBufferData.model, XMMatrixTranspose(rotationMatrix * modelTranslation));

        if (m_fadeTime > 0.f)
        {
            // Fade the quad in, or out.
            if (m_fadingIn)
            {
                const float fadeLerp = 1.f - (m_fadeTime / c_maxFadeTime);
                m_modelConstantBufferData.hologramColorFadeMultiplier = XMFLOAT4(fadeLerp, fadeLerp, fadeLerp, 1.f);
            }
            else
            {
                const float fadeLerp = (m_fadeTime / c_maxFadeTime);
                m_modelConstantBufferData.hologramColorFadeMultiplier = XMFLOAT4(fadeLerp, fadeLerp, fadeLerp, 1.f);
            }
            m_fadeTime -= deltaTime;
        }
        else
        {
            // Set a value directly.
            if (m_fadingIn)
            {
                m_modelConstantBufferData.hologramColorFadeMultiplier = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
            }
            else
            {
                m_modelConstantBufferData.hologramColorFadeMultiplier = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
            }
        }

        // Determine velocity.
        // Even though the motion is spherical, the velocity is still linear
        // for image stabilization.
        const float3 deltaPosition = m_position - m_lastPosition; // meters
        m_velocity = deltaPosition * oneOverDeltaTime; // meters per second

        // Loading is asynchronous. Resources must be created before they can be updated.
        if (!m_loadingComplete)
        {
            return;
        }

        // Use the D3D device context to update Direct3D device-based resources.
        const auto context = m_deviceResources->GetD3DDeviceContext();

        // Update the model transform buffer for the hologram.
        context->UpdateSubresource(
            m_modelConstantBuffer.Get(),
            0,
            nullptr,
            &m_modelConstantBufferData,
            0,
            0
        );
    }

    // Renders one frame using the vertex and pixel shaders.
    // For Direct3D devices that do not support the optional feature level
    // 11.3 VPRT feature, this function also uses a geometry shader.
    void QuadRenderer::Render()
    {
        // Loading is asynchronous. Resources must be created before drawing can occur.
        if (!m_loadingComplete)
        {
            return;
        }

        const auto context = m_deviceResources->GetD3DDeviceContext();

        // Each vertex is one instance of the VertexPositionColor struct.
        const UINT stride = sizeof(VertexPositionColorTex);
        const UINT offset = 0;
        context->IASetVertexBuffers(
            0,
            1,
            m_vertexBuffer.GetAddressOf(),
            &stride,
            &offset
        );
        context->IASetIndexBuffer(
            m_indexBuffer.Get(),
            DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
            0
        );
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(m_inputLayout.Get());

        // Attach the vertex shader.
        context->VSSetShader(
            m_vertexShader.Get(),
            nullptr,
            0
        );
        // Apply the model constant buffer to the vertex shader.
        context->VSSetConstantBuffers(
            0,
            1,
            m_modelConstantBuffer.GetAddressOf()
        );

        if (!m_usingVprtShaders)
        {
            // On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
            // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
            // a pass-through geometry shader sets the render target ID.
            context->GSSetShader(
                m_geometryShader.Get(),
                nullptr,
                0
            );
        }

        // Attach the pixel shader.
        context->PSSetShader(
            m_pixelShader.Get(),
            nullptr,
            0
        );
        context->PSSetShaderResources(
            0,
            1,
            m_quadTextureView.GetAddressOf()
        );
        context->PSSetSamplers(
            0,
            1,
            m_quadTextureSamplerState.GetAddressOf()
        );

        // Draw the objects.
        context->DrawIndexedInstanced(
            m_indexCount,   // Index count per instance.
            2,              // Instance count.
            0,              // Start index location.
            0,              // Base vertex location.
            0               // Start instance location.
        );
    }


    void QuadRenderer::StartFadeIn()
    {
        if (!m_fadingIn)
        {
            m_fadeTime = c_maxFadeTime;
            m_fadingIn = true;
        }
    }

    void QuadRenderer::StartFadeOut()
    {
        if (m_fadingIn)
        {
            m_fadeTime = c_maxFadeTime;
            m_fadingIn = false;
        }
    }

    void QuadRenderer::Resize(int width, int height)
    {
        m_loadingComplete = false;
        m_quadTexture.Reset();
        m_quadTextureView.Reset();
        m_quadTextureSamplerState.Reset();
        m_vertexBuffer.Reset();
        m_indexBuffer.Reset();
        m_width = width;
        m_height = height;
        CreateSharedTextureQuad();
        m_loadingComplete = true;
    }

    void QuadRenderer::CreateSharedTextureQuad()
    {
        // Load mesh vertices. Each vertex has a position and a color.
        // Note that the quad size has changed from the default DirectX app
        // template. Windows Holographic is scaled in meters, so to draw the
        // quad at a comfortable size we made the quad width 0.2 m (20 cm).

        std::vector<VertexPositionColorTex> quadVertices;
        float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
        float quadWidth = .999f;
        float quadHeight = quadWidth / aspectRatio;

        // note need to flip tex coords for screen capture image
        quadVertices.push_back({ XMFLOAT3(-quadWidth / 2.0f,  quadHeight / 2.0f, 0.f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.f, 1.0f - 0.f) });
        quadVertices.push_back({ XMFLOAT3(quadWidth / 2.0f,  quadHeight / 2.0f, 0.f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.f, 1.0f - 0.f) });
        quadVertices.push_back({ XMFLOAT3(quadWidth / 2.0f, -quadHeight / 2.0f, 0.f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.f, 1.0f - 1.f) });
        quadVertices.push_back({ XMFLOAT3(-quadWidth / 2.0f, -quadHeight / 2.0f, 0.f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.f, 1.0f - 1.f) });

        D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
        vertexBufferData.pSysMem = quadVertices.data();
        vertexBufferData.SysMemPitch = 0;
        vertexBufferData.SysMemSlicePitch = 0;
        const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionColorTex) * quadVertices.size(), D3D11_BIND_VERTEX_BUFFER);
        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateBuffer(
                &vertexBufferDesc,
                &vertexBufferData,
                &m_vertexBuffer
            )
        );

        // Load mesh indices. Each trio of indices represents
        // a triangle to be rendered on the screen.
        // For example: 2,1,0 means that the vertices with indexes
        // 2, 1, and 0 from the vertex buffer compose the
        // first triangle of this mesh.
        // Note that the winding order is clockwise by default.
        constexpr std::array<unsigned short, 12> quadIndices =
        {
            {
                // -z
                0, 2, 3,
                0, 1, 2,

                // +z
                2, 0, 3,
                1, 0, 2,
            }
        };

        m_indexCount = static_cast<UINT>(quadIndices.size());

        D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
        indexBufferData.pSysMem = quadIndices.data();
        indexBufferData.SysMemPitch = 0;
        indexBufferData.SysMemSlicePitch = 0;
        const CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned short) * quadIndices.size(), D3D11_BIND_INDEX_BUFFER);
        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateBuffer(
                &indexBufferDesc,
                &indexBufferData,
                &m_indexBuffer
            )
        );

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = m_width;
        desc.Height = m_height;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        ID3D11Texture2D *pTexture = NULL;
        auto hr = m_deviceResources->GetD3DDevice()->CreateTexture2D(&desc, nullptr, &pTexture);

        m_quadTexture = pTexture;


        // QI IDXGIResource interface to synchronized shared surface.
        m_sharedTextureHandle = NULL;
        IDXGIResource* pDXGIResource = NULL;
        m_quadTexture->QueryInterface(__uuidof(IDXGIResource), (LPVOID*)&pDXGIResource);

        // obtain handle to IDXGIResource object.
        pDXGIResource->GetSharedHandle(&m_sharedTextureHandle);
        pDXGIResource->Release();

        UINT_PTR f = (UINT_PTR)m_sharedTextureHandle;

        HANDLE h = (HANDLE)f;

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = desc.Format;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = 1;

        m_deviceResources->GetD3DDevice()->CreateShaderResourceView(m_quadTexture.Get(), &SRVDesc, m_quadTextureView.GetAddressOf());

        D3D11_SAMPLER_DESC samplesDesc;
        ZeroMemory(&samplesDesc, sizeof(D3D11_SAMPLER_DESC));

        samplesDesc.Filter = D3D11_FILTER_ANISOTROPIC;
        samplesDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplesDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplesDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplesDesc.MaxAnisotropy = 3;
        samplesDesc.MinLOD = 0;
        samplesDesc.MaxLOD = 3;
        samplesDesc.MipLODBias = 0.f;
        samplesDesc.BorderColor[0] = 0.f;
        samplesDesc.BorderColor[1] = 0.f;
        samplesDesc.BorderColor[2] = 0.f;
        samplesDesc.BorderColor[3] = 0.f;
        samplesDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

        DX::ThrowIfFailed(
            m_deviceResources->GetD3DDevice()->CreateSamplerState(
                &samplesDesc,
                &m_quadTextureSamplerState
            )
        );

        m_deviceResources->GetD3DDevice()->CreateShaderResourceView(m_quadTexture.Get(), &SRVDesc, m_quadTextureView.GetAddressOf());

    }

    
    void QuadRenderer::CreateDeviceDependentResources()
    {
        m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

        // If the optional VPRT feature is supported by the graphics device, we
        // can avoid using geometry shaders to set the render target array index.
        std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///VprtVertexShader.cso" : L"ms-appx:///VertexShader.cso";

        // Load shaders asynchronously.
        task<std::vector<byte>> loadVSTask = DX::ReadDataAsync(vertexShaderFileName);
        task<std::vector<byte>> loadPSTask = DX::ReadDataAsync(L"ms-appx:///PixelShader.cso");

        task<std::vector<byte>> loadGSTask;
        if (!m_usingVprtShaders)
        {
            // Load the pass-through geometry shader.
            loadGSTask = DX::ReadDataAsync(L"ms-appx:///GeometryShader.cso");
        }

        // After the vertex shader file is loaded, create the shader and input layout.
        task<void> createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData)
        {
            DX::ThrowIfFailed(
                m_deviceResources->GetD3DDevice()->CreateVertexShader(
                    fileData.data(),
                    fileData.size(),
                    nullptr,
                    &m_vertexShader
                )
            );

            constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 3> vertexDesc =
            {
              {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
              }
            };

            DX::ThrowIfFailed(
                m_deviceResources->GetD3DDevice()->CreateInputLayout(
                    vertexDesc.data(),
                    static_cast<UINT>(vertexDesc.size()),
                    fileData.data(),
                    static_cast<UINT>(fileData.size()),
                    &m_inputLayout
                )
            );
        });

        // After the pixel shader file is loaded, create the shader and constant buffer.
        task<void> createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData)
        {
            DX::ThrowIfFailed(
                m_deviceResources->GetD3DDevice()->CreatePixelShader(
                    fileData.data(),
                    fileData.size(),
                    nullptr,
                    &m_pixelShader
                )
            );

            const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
            DX::ThrowIfFailed(
                m_deviceResources->GetD3DDevice()->CreateBuffer(
                    &constantBufferDesc,
                    nullptr,
                    &m_modelConstantBuffer
                )
            );
        });

        task<void> createGSTask;
        if (!m_usingVprtShaders)
        {
            // After the geometry shader file is loaded, create the shader.
            createGSTask = loadGSTask.then([this](const std::vector<byte>& fileData)
            {
                DX::ThrowIfFailed(
                    m_deviceResources->GetD3DDevice()->CreateGeometryShader(
                        fileData.data(),
                        fileData.size(),
                        nullptr,
                        &m_geometryShader
                    )
                );
            });
        }

        // Once all shaders are loaded, create the mesh.
        task<void> shaderTaskGroup = m_usingVprtShaders ? (createPSTask && createVSTask) : (createPSTask && createVSTask && createGSTask);
        task<void> finishLoadingTask = shaderTaskGroup.then([this]()
        {
            CreateSharedTextureQuad();

            // After the assets are loaded, the quad is ready to be rendered.
            m_loadingComplete = true;
        });
    }

    void QuadRenderer::ReleaseDeviceDependentResources()
    {
        m_loadingComplete = false;
        m_usingVprtShaders = false;

        m_vertexShader.Reset();
        m_inputLayout.Reset();
        m_pixelShader.Reset();
        m_geometryShader.Reset();

        m_modelConstantBuffer.Reset();

        m_vertexBuffer.Reset();
        m_indexBuffer.Reset();

        m_quadTexture.Reset();
        m_quadTextureView.Reset();
        m_quadTextureSamplerState.Reset();
    }
}