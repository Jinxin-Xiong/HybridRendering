#include <application.h>
#include <camera.h>
#include <material.h>
#include <mesh.h>
#include <vk.h>
#include <profiler.h>
#include <assimp/scene.h>
#include <vk_mem_alloc.h>
#include <scene.h>

// Uniform buffer data structure.
struct Transforms
{
    DW_ALIGNED(16)
    glm::mat4 view_inverse;
    DW_ALIGNED(16)
    glm::mat4 proj_inverse;
    DW_ALIGNED(16)
    glm::mat4 model;
    DW_ALIGNED(16)
    glm::mat4 view;
    DW_ALIGNED(16)
    glm::mat4 proj;
    DW_ALIGNED(16)
    glm::vec4 cam_pos;
    DW_ALIGNED(16)
    glm::vec4 light_dir;
};

class Sample : public dw::Application
{
protected:
    // -----------------------------------------------------------------------------------------------------------------------------------

    bool init(int argc, const char* argv[]) override
    {
        // Create GPU resources.
        if (!create_shaders())
            return false;

        if (!create_uniform_buffer())
            return false;

        // Load mesh.
        if (!load_mesh())
        {
            DW_LOG_INFO("Failed to load mesh");
            return false;
        }

        load_blue_noise();
        create_output_images();
        create_render_passes();
        create_framebuffers();
        create_descriptor_set_layouts();
        create_descriptor_sets();
        write_descriptor_sets();
        create_deferred_pipeline();
        create_gbuffer_pipeline();
        create_shadow_mask_ray_tracing_pipeline();
        create_reflection_ray_tracing_pipeline();

        // Create camera.
        create_camera();

        m_light_direction = glm::normalize(glm::vec3(0.2f, 0.9770f, 0.2f));

        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void update(double delta) override
    {
        dw::vk::CommandBuffer::Ptr cmd_buf = m_vk_backend->allocate_graphics_command_buffer();

        VkCommandBufferBeginInfo begin_info;
        DW_ZERO_MEMORY(begin_info);

        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(cmd_buf->handle(), &begin_info);

        {
            DW_SCOPED_SAMPLE("update", cmd_buf);

            // Render profiler.
            //dw::profiler::ui();

            // Update camera.
            update_camera();

            // Update uniforms.
            update_uniforms(cmd_buf);

            // Render.
            render_gbuffer(cmd_buf);
            ray_trace_shadow_mask(cmd_buf);
            ray_trace_reflection(cmd_buf);

            render(cmd_buf);
        }

        vkEndCommandBuffer(cmd_buf->handle());

        submit_and_present({ cmd_buf });
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void shutdown() override
    {
        m_blue_noise.reset();
        m_blue_noise_view.reset();
        m_reflection_ds.reset();
        m_deferred_ds.reset();
        m_per_frame_ds.reset();
        m_g_buffer_ds.reset();
        m_shadow_mask_ds.reset();
        m_per_frame_ds_layout.reset();
        m_g_buffer_ds_layout.reset();
        m_reflection_ds_layout.reset();
        m_shadow_mask_ds_layout.reset();
        m_shadow_mask_pipeline_layout.reset();
        m_reflection_pipeline_layout.reset();
        m_g_buffer_pipeline_layout.reset();
        m_deferred_layout.reset();
        m_deferred_pipeline_layout.reset();
        m_ubo.reset();
        m_deferred_pipeline.reset();
        m_shadow_mask_pipeline.reset();
        m_g_buffer_pipeline.reset();
        m_reflection_pipeline.reset();
        m_g_buffer_fbo.reset();
        m_g_buffer_rp.reset();
        m_reflection_view.reset();
        m_shadow_mask_view.reset();
        m_g_buffer_1_view.reset();
        m_g_buffer_2_view.reset();
        m_g_buffer_3_view.reset();
        m_blue_noise_view.reset();
        m_g_buffer_depth_view.reset();
        m_shadow_mask_image.reset();
        m_reflection_image.reset();
        m_blue_noise.reset();
        m_g_buffer_1.reset();
        m_g_buffer_2.reset();
        m_g_buffer_3.reset();
        m_g_buffer_depth.reset();
        m_shadow_mask_sbt.reset();
        m_reflection_sbt.reset();

        // Unload assets.
        m_scene.reset();
        m_mesh.reset();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void key_pressed(int code) override
    {
        // Handle forward movement.
        if (code == GLFW_KEY_W)
            m_heading_speed = m_camera_speed;
        else if (code == GLFW_KEY_S)
            m_heading_speed = -m_camera_speed;

        // Handle sideways movement.
        if (code == GLFW_KEY_A)
            m_sideways_speed = -m_camera_speed;
        else if (code == GLFW_KEY_D)
            m_sideways_speed = m_camera_speed;

        if (code == GLFW_KEY_SPACE)
            m_mouse_look = true;

        if (code == GLFW_KEY_G)
            m_debug_gui = !m_debug_gui;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void key_released(int code) override
    {
        // Handle forward movement.
        if (code == GLFW_KEY_W || code == GLFW_KEY_S)
            m_heading_speed = 0.0f;

        // Handle sideways movement.
        if (code == GLFW_KEY_A || code == GLFW_KEY_D)
            m_sideways_speed = 0.0f;

        if (code == GLFW_KEY_SPACE)
            m_mouse_look = false;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void mouse_pressed(int code) override
    {
        // Enable mouse look.
        if (code == GLFW_MOUSE_BUTTON_RIGHT)
            m_mouse_look = true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void mouse_released(int code) override
    {
        // Disable mouse look.
        if (code == GLFW_MOUSE_BUTTON_RIGHT)
            m_mouse_look = false;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    dw::AppSettings intial_app_settings() override
    {
        // Set custom settings here...
        dw::AppSettings settings;

        settings.width       = 1920;
        settings.height      = 1080;
        settings.title       = "Hybrid Rendering (c) Dihara Wijetunga";
        settings.ray_tracing = true;

        return settings;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void window_resized(int width, int height) override
    {
        // Override window resized method to update camera projection.
        m_main_camera->update_projection(60.0f, 0.1f, 10000.0f, float(m_width) / float(m_height));

        m_vk_backend->wait_idle();

        create_output_images();
        create_framebuffers();
        write_descriptor_sets();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

private:
    // -----------------------------------------------------------------------------------------------------------------------------------

    bool create_shaders()
    {
        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_output_images()
    {
        m_shadow_mask_image.reset();
        m_shadow_mask_view.reset();
        m_reflection_image.reset();
        m_reflection_view.reset();
        m_g_buffer_1.reset();
        m_g_buffer_2.reset();
        m_g_buffer_3.reset();
        m_g_buffer_depth.reset();
        m_g_buffer_1_view.reset();
        m_g_buffer_2_view.reset();
        m_g_buffer_3_view.reset();
        m_g_buffer_depth_view.reset();

        m_shadow_mask_image = dw::vk::Image::create(m_vk_backend, VK_IMAGE_TYPE_2D, m_width, m_height, 1, 1, 1, VK_FORMAT_R8_SNORM, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
        m_shadow_mask_view  = dw::vk::ImageView::create(m_vk_backend, m_shadow_mask_image, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

        m_reflection_image = dw::vk::Image::create(m_vk_backend, VK_IMAGE_TYPE_2D, m_width, m_height, 1, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
        m_reflection_view  = dw::vk::ImageView::create(m_vk_backend, m_reflection_image, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

        m_g_buffer_1     = dw::vk::Image::create(m_vk_backend, VK_IMAGE_TYPE_2D, m_width, m_height, 1, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT);
        m_g_buffer_2     = dw::vk::Image::create(m_vk_backend, VK_IMAGE_TYPE_2D, m_width, m_height, 1, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT);
        m_g_buffer_3     = dw::vk::Image::create(m_vk_backend, VK_IMAGE_TYPE_2D, m_width, m_height, 1, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT);
        m_g_buffer_depth = dw::vk::Image::create(m_vk_backend, VK_IMAGE_TYPE_2D, m_width, m_height, 1, 1, 1, m_vk_backend->swap_chain_depth_format(), VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT);

        m_g_buffer_1_view     = dw::vk::ImageView::create(m_vk_backend, m_g_buffer_1, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
        m_g_buffer_2_view     = dw::vk::ImageView::create(m_vk_backend, m_g_buffer_2, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
        m_g_buffer_3_view     = dw::vk::ImageView::create(m_vk_backend, m_g_buffer_3, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
        m_g_buffer_depth_view = dw::vk::ImageView::create(m_vk_backend, m_g_buffer_depth, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_render_passes()
    {
        std::vector<VkAttachmentDescription> attachments(4);

        // GBuffer1 attachment
        attachments[0].format         = VK_FORMAT_R8G8B8A8_UNORM;
        attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // GBuffer2 attachment
        attachments[1].format         = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // GBuffer3 attachment
        attachments[2].format         = VK_FORMAT_R32G32B32A32_SFLOAT;
        attachments[2].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[2].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[2].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[2].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[2].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[2].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Depth attachment
        attachments[3].format         = m_vk_backend->swap_chain_depth_format();
        attachments[3].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[3].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[3].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[3].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[3].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[3].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference gbuffer_references[3];

        gbuffer_references[0].attachment = 0;
        gbuffer_references[0].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        gbuffer_references[1].attachment = 1;
        gbuffer_references[1].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        gbuffer_references[2].attachment = 2;
        gbuffer_references[2].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_reference;
        depth_reference.attachment = 3;
        depth_reference.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::vector<VkSubpassDescription> subpass_description(1);

        subpass_description[0].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_description[0].colorAttachmentCount    = 3;
        subpass_description[0].pColorAttachments       = gbuffer_references;
        subpass_description[0].pDepthStencilAttachment = &depth_reference;
        subpass_description[0].inputAttachmentCount    = 0;
        subpass_description[0].pInputAttachments       = nullptr;
        subpass_description[0].preserveAttachmentCount = 0;
        subpass_description[0].pPreserveAttachments    = nullptr;
        subpass_description[0].pResolveAttachments     = nullptr;

        // Subpass dependencies for layout transitions
        std::vector<VkSubpassDependency> dependencies(2);

        dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass      = 0;
        dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass      = 0;
        dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        m_g_buffer_rp = dw::vk::RenderPass::create(m_vk_backend, attachments, subpass_description, dependencies);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_framebuffers()
    {
        m_g_buffer_fbo.reset();
        m_g_buffer_fbo = dw::vk::Framebuffer::create(m_vk_backend, m_g_buffer_rp, { m_g_buffer_1_view, m_g_buffer_2_view, m_g_buffer_3_view, m_g_buffer_depth_view }, m_width, m_height, 1);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    bool create_uniform_buffer()
    {
        m_ubo_size = m_vk_backend->aligned_dynamic_ubo_size(sizeof(Transforms));
        m_ubo      = dw::vk::Buffer::create(m_vk_backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_ubo_size * dw::vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_descriptor_set_layouts()
    {
        {
            dw::vk::DescriptorSetLayout::Desc desc;

            desc.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
            desc.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
            desc.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
            desc.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
            desc.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);

            m_deferred_layout = dw::vk::DescriptorSetLayout::create(m_vk_backend, desc);
        }

        {
            dw::vk::DescriptorSetLayout::Desc desc;

            desc.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT);
            desc.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT);
            desc.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT);

            m_g_buffer_ds_layout = dw::vk::DescriptorSetLayout::create(m_vk_backend, desc);
        }

        {
            dw::vk::DescriptorSetLayout::Desc desc;

            desc.add_binding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
            desc.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV);

            m_shadow_mask_ds_layout = dw::vk::DescriptorSetLayout::create(m_vk_backend, desc);
        }

        {
            dw::vk::DescriptorSetLayout::Desc desc;

            desc.add_binding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
            desc.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV);
            desc.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV);

            m_reflection_ds_layout = dw::vk::DescriptorSetLayout::create(m_vk_backend, desc);
        }

        {
            dw::vk::DescriptorSetLayout::Desc desc;

            desc.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

            m_per_frame_ds_layout = dw::vk::DescriptorSetLayout::create(m_vk_backend, desc);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_descriptor_sets()
    {
        m_deferred_ds = m_vk_backend->allocate_descriptor_set(m_deferred_layout);
        m_per_frame_ds = m_vk_backend->allocate_descriptor_set(m_per_frame_ds_layout);
        m_g_buffer_ds = m_vk_backend->allocate_descriptor_set(m_g_buffer_ds_layout);
        m_shadow_mask_ds = m_vk_backend->allocate_descriptor_set(m_shadow_mask_ds_layout);
        m_reflection_ds  = m_vk_backend->allocate_descriptor_set(m_reflection_ds_layout);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void write_descriptor_sets()
    {
        {
            VkDescriptorImageInfo image_info[5];

            image_info[0].sampler     = dw::Material::common_sampler()->handle();
            image_info[0].imageView   = m_shadow_mask_view->handle();
            image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            image_info[1].sampler     = dw::Material::common_sampler()->handle();
            image_info[1].imageView   = m_reflection_view->handle();
            image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            image_info[2].sampler     = dw::Material::common_sampler()->handle();
            image_info[2].imageView   = m_g_buffer_1_view->handle();
            image_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            image_info[3].sampler     = dw::Material::common_sampler()->handle();
            image_info[3].imageView   = m_g_buffer_2_view->handle();
            image_info[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            image_info[4].sampler     = dw::Material::common_sampler()->handle();
            image_info[4].imageView   = m_g_buffer_3_view->handle();
            image_info[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write_data[5];
            DW_ZERO_MEMORY(write_data[0]);
            DW_ZERO_MEMORY(write_data[1]);
            DW_ZERO_MEMORY(write_data[2]);
            DW_ZERO_MEMORY(write_data[3]);
            DW_ZERO_MEMORY(write_data[4]);

            write_data[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[0].descriptorCount = 1;
            write_data[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data[0].pImageInfo      = &image_info[0];
            write_data[0].dstBinding      = 0;
            write_data[0].dstSet          = m_deferred_ds->handle();

            write_data[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[1].descriptorCount = 1;
            write_data[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data[1].pImageInfo      = &image_info[1];
            write_data[1].dstBinding      = 1;
            write_data[1].dstSet          = m_deferred_ds->handle();

            write_data[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[2].descriptorCount = 1;
            write_data[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data[2].pImageInfo      = &image_info[2];
            write_data[2].dstBinding      = 2;
            write_data[2].dstSet          = m_deferred_ds->handle();

            write_data[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[3].descriptorCount = 1;
            write_data[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data[3].pImageInfo      = &image_info[3];
            write_data[3].dstBinding      = 3;
            write_data[3].dstSet          = m_deferred_ds->handle();

            write_data[4].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[4].descriptorCount = 1;
            write_data[4].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data[4].pImageInfo      = &image_info[4];
            write_data[4].dstBinding      = 4;
            write_data[4].dstSet          = m_deferred_ds->handle();

            vkUpdateDescriptorSets(m_vk_backend->device(), 5, &write_data[0], 0, nullptr);
        }

        {
            VkDescriptorBufferInfo buffer_info;

            buffer_info.range  = VK_WHOLE_SIZE;
            buffer_info.offset = 0;
            buffer_info.buffer = m_ubo->handle();

            VkWriteDescriptorSet write_data;
            DW_ZERO_MEMORY(write_data);

            write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data.descriptorCount = 1;
            write_data.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            write_data.pBufferInfo     = &buffer_info;
            write_data.dstBinding      = 0;
            write_data.dstSet          = m_per_frame_ds->handle();

            vkUpdateDescriptorSets(m_vk_backend->device(), 1, &write_data, 0, nullptr);
        }

        {
            VkDescriptorImageInfo image_info[3];

            image_info[0].sampler     = dw::Material::common_sampler()->handle();
            image_info[0].imageView   = m_g_buffer_1_view->handle();
            image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            image_info[1].sampler     = dw::Material::common_sampler()->handle();
            image_info[1].imageView   = m_g_buffer_2_view->handle();
            image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            image_info[2].sampler     = dw::Material::common_sampler()->handle();
            image_info[2].imageView   = m_g_buffer_3_view->handle();
            image_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write_data[3];
            DW_ZERO_MEMORY(write_data[0]);
            DW_ZERO_MEMORY(write_data[1]);
            DW_ZERO_MEMORY(write_data[2]);

            write_data[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[0].descriptorCount = 1;
            write_data[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data[0].pImageInfo      = &image_info[0];
            write_data[0].dstBinding      = 0;
            write_data[0].dstSet          = m_g_buffer_ds->handle();

            write_data[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[1].descriptorCount = 1;
            write_data[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data[1].pImageInfo      = &image_info[1];
            write_data[1].dstBinding      = 1;
            write_data[1].dstSet          = m_g_buffer_ds->handle();

            write_data[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[2].descriptorCount = 1;
            write_data[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data[2].pImageInfo      = &image_info[2];
            write_data[2].dstBinding      = 2;
            write_data[2].dstSet          = m_g_buffer_ds->handle();

            vkUpdateDescriptorSets(m_vk_backend->device(), 3, &write_data[0], 0, nullptr);
        }

        {
            VkWriteDescriptorSet write_data[2];
            DW_ZERO_MEMORY(write_data[0]);
            DW_ZERO_MEMORY(write_data[1]);

            VkWriteDescriptorSetAccelerationStructureNV descriptor_as;

            descriptor_as.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
            descriptor_as.pNext                      = nullptr;
            descriptor_as.accelerationStructureCount = 1;
            descriptor_as.pAccelerationStructures    = &m_scene->acceleration_structure()->handle();

            write_data[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[0].pNext           = &descriptor_as;
            write_data[0].descriptorCount = 1;
            write_data[0].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
            write_data[0].dstBinding      = 0;
            write_data[0].dstSet          = m_shadow_mask_ds->handle();

            VkDescriptorImageInfo output_image;
            output_image.sampler     = VK_NULL_HANDLE;
            output_image.imageView   = m_shadow_mask_view->handle();
            output_image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            write_data[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[1].descriptorCount = 1;
            write_data[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write_data[1].pImageInfo      = &output_image;
            write_data[1].dstBinding      = 1;
            write_data[1].dstSet          = m_shadow_mask_ds->handle();

            vkUpdateDescriptorSets(m_vk_backend->device(), 2, &write_data[0], 0, nullptr);
        }

        {
            VkWriteDescriptorSet write_data[3];
            DW_ZERO_MEMORY(write_data[0]);
            DW_ZERO_MEMORY(write_data[1]);
            DW_ZERO_MEMORY(write_data[2]);

            VkWriteDescriptorSetAccelerationStructureNV descriptor_as;

            descriptor_as.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
            descriptor_as.pNext                      = nullptr;
            descriptor_as.accelerationStructureCount = 1;
            descriptor_as.pAccelerationStructures    = &m_scene->acceleration_structure()->handle();

            write_data[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[0].pNext           = &descriptor_as;
            write_data[0].descriptorCount = 1;
            write_data[0].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
            write_data[0].dstBinding      = 0;
            write_data[0].dstSet          = m_reflection_ds->handle();

            VkDescriptorImageInfo output_image;
            output_image.sampler     = VK_NULL_HANDLE;
            output_image.imageView   = m_reflection_view->handle();
            output_image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            write_data[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[1].descriptorCount = 1;
            write_data[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write_data[1].pImageInfo      = &output_image;
            write_data[1].dstBinding      = 1;
            write_data[1].dstSet          = m_reflection_ds->handle();

            VkDescriptorImageInfo blue_noise_image;
            blue_noise_image.sampler     = dw::Material::common_sampler()->handle();
            blue_noise_image.imageView   = m_blue_noise_view->handle();
            blue_noise_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            write_data[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_data[2].descriptorCount = 1;
            write_data[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_data[2].pImageInfo      = &blue_noise_image;
            write_data[2].dstBinding      = 2;
            write_data[2].dstSet          = m_reflection_ds->handle();

            vkUpdateDescriptorSets(m_vk_backend->device(), 3, &write_data[0], 0, nullptr);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_deferred_pipeline()
    {
        dw::vk::PipelineLayout::Desc desc;

        desc.add_descriptor_set_layout(m_deferred_layout);
        desc.add_descriptor_set_layout(m_per_frame_ds_layout);

        m_deferred_pipeline_layout = dw::vk::PipelineLayout::create(m_vk_backend, desc);
        m_deferred_pipeline        = dw::vk::GraphicsPipeline::create_for_post_process(m_vk_backend, "shaders/triangle.vert.spv", "shaders/deferred.frag.spv", m_deferred_pipeline_layout, m_vk_backend->swapchain_render_pass());
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_shadow_mask_ray_tracing_pipeline()
    {
        // ---------------------------------------------------------------------------
        // Create shader modules
        // ---------------------------------------------------------------------------

        dw::vk::ShaderModule::Ptr rgen  = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/shadow.rgen.spv");
        dw::vk::ShaderModule::Ptr rchit = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/shadow.rchit.spv");
        dw::vk::ShaderModule::Ptr rmiss = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/shadow.rmiss.spv");

        dw::vk::ShaderBindingTable::Desc sbt_desc;

        sbt_desc.add_ray_gen_group(rgen, "main");
        sbt_desc.add_hit_group(rchit, "main");
        sbt_desc.add_miss_group(rmiss, "main");

        m_shadow_mask_sbt = dw::vk::ShaderBindingTable::create(m_vk_backend, sbt_desc);

        dw::vk::RayTracingPipeline::Desc desc;

        desc.set_recursion_depth(1);
        desc.set_shader_binding_table(m_shadow_mask_sbt);

        // ---------------------------------------------------------------------------
        // Create pipeline layout
        // ---------------------------------------------------------------------------

        dw::vk::PipelineLayout::Desc pl_desc;

        pl_desc.add_descriptor_set_layout(m_shadow_mask_ds_layout);
        pl_desc.add_descriptor_set_layout(m_per_frame_ds_layout);
        pl_desc.add_descriptor_set_layout(m_g_buffer_ds_layout);

        m_shadow_mask_pipeline_layout = dw::vk::PipelineLayout::create(m_vk_backend, pl_desc);

        desc.set_pipeline_layout(m_shadow_mask_pipeline_layout);

        m_shadow_mask_pipeline = dw::vk::RayTracingPipeline::create(m_vk_backend, desc);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_reflection_ray_tracing_pipeline()
    {
        // ---------------------------------------------------------------------------
        // Create shader modules
        // ---------------------------------------------------------------------------

        dw::vk::ShaderModule::Ptr rgen  = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/reflection.rgen.spv");
        dw::vk::ShaderModule::Ptr rchit = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/reflection.rchit.spv");
        dw::vk::ShaderModule::Ptr rmiss = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/reflection.rmiss.spv");

        dw::vk::ShaderBindingTable::Desc sbt_desc;

        sbt_desc.add_ray_gen_group(rgen, "main");
        sbt_desc.add_hit_group(rchit, "main");
        sbt_desc.add_miss_group(rmiss, "main");

        m_reflection_sbt = dw::vk::ShaderBindingTable::create(m_vk_backend, sbt_desc);

        dw::vk::RayTracingPipeline::Desc desc;

        desc.set_recursion_depth(1);
        desc.set_shader_binding_table(m_reflection_sbt);

        // ---------------------------------------------------------------------------
        // Create pipeline layout
        // ---------------------------------------------------------------------------

        dw::vk::PipelineLayout::Desc pl_desc;

        pl_desc.add_descriptor_set_layout(m_reflection_ds_layout);
        pl_desc.add_descriptor_set_layout(m_per_frame_ds_layout);
        pl_desc.add_descriptor_set_layout(m_g_buffer_ds_layout);
        pl_desc.add_descriptor_set_layout(m_scene->ray_tracing_geometry_descriptor_set_layout());
        pl_desc.add_descriptor_set_layout(m_scene->material_descriptor_set_layout());
        pl_desc.add_descriptor_set_layout(m_scene->material_descriptor_set_layout());
        pl_desc.add_descriptor_set_layout(m_scene->material_descriptor_set_layout());
        pl_desc.add_descriptor_set_layout(m_scene->material_descriptor_set_layout());

        m_reflection_pipeline_layout = dw::vk::PipelineLayout::create(m_vk_backend, pl_desc);

        desc.set_pipeline_layout(m_reflection_pipeline_layout);

        m_reflection_pipeline = dw::vk::RayTracingPipeline::create(m_vk_backend, desc);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_gbuffer_pipeline()
    {
        // ---------------------------------------------------------------------------
        // Create shader modules
        // ---------------------------------------------------------------------------

        dw::vk::ShaderModule::Ptr vs = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/g_buffer.vert.spv");
        dw::vk::ShaderModule::Ptr fs = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/g_buffer.frag.spv");

        dw::vk::GraphicsPipeline::Desc pso_desc;

        pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs, "main")
            .add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main");

        // ---------------------------------------------------------------------------
        // Create vertex input state
        // ---------------------------------------------------------------------------

        pso_desc.set_vertex_input_state(m_mesh->vertex_input_state_desc());

        // ---------------------------------------------------------------------------
        // Create pipeline input assembly state
        // ---------------------------------------------------------------------------

        dw::vk::InputAssemblyStateDesc input_assembly_state_desc;

        input_assembly_state_desc.set_primitive_restart_enable(false)
            .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        pso_desc.set_input_assembly_state(input_assembly_state_desc);

        // ---------------------------------------------------------------------------
        // Create viewport state
        // ---------------------------------------------------------------------------

        dw::vk::ViewportStateDesc vp_desc;

        vp_desc.add_viewport(0.0f, 0.0f, m_width, m_height, 0.0f, 1.0f)
            .add_scissor(0, 0, m_width, m_height);

        pso_desc.set_viewport_state(vp_desc);

        // ---------------------------------------------------------------------------
        // Create rasterization state
        // ---------------------------------------------------------------------------

        dw::vk::RasterizationStateDesc rs_state;

        rs_state.set_depth_clamp(VK_FALSE)
            .set_rasterizer_discard_enable(VK_FALSE)
            .set_polygon_mode(VK_POLYGON_MODE_FILL)
            .set_line_width(1.0f)
            .set_cull_mode(VK_CULL_MODE_BACK_BIT)
            .set_front_face(VK_FRONT_FACE_CLOCKWISE)
            .set_depth_bias(VK_FALSE);

        pso_desc.set_rasterization_state(rs_state);

        // ---------------------------------------------------------------------------
        // Create multisample state
        // ---------------------------------------------------------------------------

        dw::vk::MultisampleStateDesc ms_state;

        ms_state.set_sample_shading_enable(VK_FALSE)
            .set_rasterization_samples(VK_SAMPLE_COUNT_1_BIT);

        pso_desc.set_multisample_state(ms_state);

        // ---------------------------------------------------------------------------
        // Create depth stencil state
        // ---------------------------------------------------------------------------

        dw::vk::DepthStencilStateDesc ds_state;

        ds_state.set_depth_test_enable(VK_TRUE)
            .set_depth_write_enable(VK_TRUE)
            .set_depth_compare_op(VK_COMPARE_OP_LESS)
            .set_depth_bounds_test_enable(VK_FALSE)
            .set_stencil_test_enable(VK_FALSE);

        pso_desc.set_depth_stencil_state(ds_state);

        // ---------------------------------------------------------------------------
        // Create color blend state
        // ---------------------------------------------------------------------------

        dw::vk::ColorBlendAttachmentStateDesc blend_att_desc;

        blend_att_desc.set_color_write_mask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
            .set_blend_enable(VK_FALSE);

        dw::vk::ColorBlendStateDesc blend_state;

        blend_state.set_logic_op_enable(VK_FALSE)
            .set_logic_op(VK_LOGIC_OP_COPY)
            .set_blend_constants(0.0f, 0.0f, 0.0f, 0.0f)
            .add_attachment(blend_att_desc)
            .add_attachment(blend_att_desc)
            .add_attachment(blend_att_desc);

        pso_desc.set_color_blend_state(blend_state);

        // ---------------------------------------------------------------------------
        // Create pipeline layout
        // ---------------------------------------------------------------------------

        dw::vk::PipelineLayout::Desc pl_desc;

        pl_desc.add_descriptor_set_layout(m_per_frame_ds_layout)
            .add_descriptor_set_layout(dw::Material::pbr_descriptor_set_layout());

        m_g_buffer_pipeline_layout = dw::vk::PipelineLayout::create(m_vk_backend, pl_desc);

        pso_desc.set_pipeline_layout(m_g_buffer_pipeline_layout);

        // ---------------------------------------------------------------------------
        // Create dynamic state
        // ---------------------------------------------------------------------------

        pso_desc.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
            .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR);

        // ---------------------------------------------------------------------------
        // Create pipeline
        // ---------------------------------------------------------------------------

        pso_desc.set_render_pass(m_g_buffer_rp);

        m_g_buffer_pipeline = dw::vk::GraphicsPipeline::create(m_vk_backend, pso_desc);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    bool load_mesh()
    {
        m_mesh = dw::Mesh::load(m_vk_backend, "mesh/sponza.obj");
        m_mesh->initialize_for_ray_tracing(m_vk_backend);

        m_scene = dw::Scene::create();
        m_scene->add_instance(m_mesh, glm::mat4(1.0f));

        m_scene->initialize_for_ray_tracing(m_vk_backend);

        return m_mesh != nullptr;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void load_blue_noise()
    {
        m_blue_noise      = dw::vk::Image::create_from_file(m_vk_backend, "texture/LDR_RGBA_0.png");
        m_blue_noise_view = dw::vk::ImageView::create(m_vk_backend, m_blue_noise, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_camera()
    {
        m_main_camera = std::make_unique<dw::Camera>(
            60.0f, 0.1f, 10000.0f, float(m_width) / float(m_height), glm::vec3(0.0f, 35.0f, 125.0f), glm::vec3(0.0f, 0.0, -1.0f));
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void ray_trace_shadow_mask(dw::vk::CommandBuffer::Ptr cmd_buf)
    {
        DW_SCOPED_SAMPLE("ray-tracing-shadows", cmd_buf);

        VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        // Transition ray tracing output image back to general layout
        dw::vk::utilities::set_image_layout(
            cmd_buf->handle(),
            m_shadow_mask_image->handle(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            subresource_range);

        auto& rt_props = m_vk_backend->ray_tracing_properties();

        vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_shadow_mask_pipeline->handle());

        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_shadow_mask_pipeline_layout->handle(), 0, 1, &m_shadow_mask_ds->handle(), 0, nullptr);

        const uint32_t dynamic_offset = m_ubo_size * m_vk_backend->current_frame_idx();

        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_shadow_mask_pipeline_layout->handle(), 1, 1, &m_per_frame_ds->handle(), 1, &dynamic_offset);

        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_shadow_mask_pipeline_layout->handle(), 2, 1, &m_g_buffer_ds->handle(), 0, VK_NULL_HANDLE);

        vkCmdTraceRaysNV(cmd_buf->handle(),
                         m_shadow_mask_pipeline->shader_binding_table_buffer()->handle(),
                         0,
                         m_shadow_mask_pipeline->shader_binding_table_buffer()->handle(),
                         m_shadow_mask_sbt->miss_group_offset(),
                         rt_props.shaderGroupHandleSize,
                         m_shadow_mask_pipeline->shader_binding_table_buffer()->handle(),
                         m_shadow_mask_sbt->hit_group_offset(),
                         rt_props.shaderGroupHandleSize,
                         VK_NULL_HANDLE,
                         0,
                         0,
                         m_width,
                         m_height,
                         1);

        // Prepare ray tracing output image as transfer source
        dw::vk::utilities::set_image_layout(
            cmd_buf->handle(),
            m_shadow_mask_image->handle(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresource_range);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void ray_trace_reflection(dw::vk::CommandBuffer::Ptr cmd_buf)
    {
        DW_SCOPED_SAMPLE("ray-tracing-reflections", cmd_buf);

        VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        // Transition ray tracing output image back to general layout
        dw::vk::utilities::set_image_layout(
            cmd_buf->handle(),
            m_reflection_image->handle(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            subresource_range);

        auto& rt_props = m_vk_backend->ray_tracing_properties();

        vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_reflection_pipeline->handle());

        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_reflection_pipeline_layout->handle(), 0, 1, &m_reflection_ds->handle(), 0, nullptr);

        const uint32_t dynamic_offset = m_ubo_size * m_vk_backend->current_frame_idx();

        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_reflection_pipeline_layout->handle(), 1, 1, &m_per_frame_ds->handle(), 1, &dynamic_offset);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_reflection_pipeline_layout->handle(), 2, 1, &m_g_buffer_ds->handle(), 0, VK_NULL_HANDLE);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_reflection_pipeline_layout->handle(), 3, 1, &m_scene->ray_tracing_geometry_descriptor_set()->handle(), 0, VK_NULL_HANDLE);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_reflection_pipeline_layout->handle(), 4, 1, &m_scene->albedo_descriptor_set()->handle(), 0, VK_NULL_HANDLE);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_reflection_pipeline_layout->handle(), 5, 1, &m_scene->normal_descriptor_set()->handle(), 0, VK_NULL_HANDLE);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_reflection_pipeline_layout->handle(), 6, 1, &m_scene->roughness_descriptor_set()->handle(), 0, VK_NULL_HANDLE);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_reflection_pipeline_layout->handle(), 7, 1, &m_scene->metallic_descriptor_set()->handle(), 0, VK_NULL_HANDLE);

        vkCmdTraceRaysNV(cmd_buf->handle(),
                         m_reflection_pipeline->shader_binding_table_buffer()->handle(),
                         0,
                         m_reflection_pipeline->shader_binding_table_buffer()->handle(),
                         m_reflection_sbt->miss_group_offset(),
                         rt_props.shaderGroupHandleSize,
                         m_reflection_pipeline->shader_binding_table_buffer()->handle(),
                         m_reflection_sbt->hit_group_offset(),
                         rt_props.shaderGroupHandleSize,
                         VK_NULL_HANDLE,
                         0,
                         0,
                         m_width,
                         m_height,
                         1);

        // Prepare ray tracing output image as transfer source
        dw::vk::utilities::set_image_layout(
            cmd_buf->handle(),
            m_reflection_image->handle(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresource_range);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void render_gbuffer(dw::vk::CommandBuffer::Ptr cmd_buf)
    {
        DW_SCOPED_SAMPLE("render_gbuffer", cmd_buf);

        VkClearValue clear_values[4];

        clear_values[0].color.float32[0] = 0.0f;
        clear_values[0].color.float32[1] = 0.0f;
        clear_values[0].color.float32[2] = 0.0f;
        clear_values[0].color.float32[3] = 1.0f;

        clear_values[1].color.float32[0] = 0.0f;
        clear_values[1].color.float32[1] = 0.0f;
        clear_values[1].color.float32[2] = 0.0f;
        clear_values[1].color.float32[3] = 1.0f;

        clear_values[2].color.float32[0] = 0.0f;
        clear_values[2].color.float32[1] = 0.0f;
        clear_values[2].color.float32[2] = 0.0f;
        clear_values[2].color.float32[3] = 1.0f;

        clear_values[3].color.float32[0] = 1.0f;
        clear_values[3].color.float32[1] = 1.0f;
        clear_values[3].color.float32[2] = 1.0f;
        clear_values[3].color.float32[3] = 1.0f;

        VkRenderPassBeginInfo info    = {};
        info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass               = m_g_buffer_rp->handle();
        info.framebuffer              = m_g_buffer_fbo->handle();
        info.renderArea.extent.width  = m_width;
        info.renderArea.extent.height = m_height;
        info.clearValueCount          = 4;
        info.pClearValues             = &clear_values[0];

        vkCmdBeginRenderPass(cmd_buf->handle(), &info, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp;

        vp.x        = 0.0f;
        vp.y        = 0.0f;
        vp.width    = (float)m_width;
        vp.height   = (float)m_height;
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;

        vkCmdSetViewport(cmd_buf->handle(), 0, 1, &vp);

        VkRect2D scissor_rect;

        scissor_rect.extent.width  = m_width;
        scissor_rect.extent.height = m_height;
        scissor_rect.offset.x      = 0;
        scissor_rect.offset.y      = 0;

        vkCmdSetScissor(cmd_buf->handle(), 0, 1, &scissor_rect);

        vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_g_buffer_pipeline->handle());

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd_buf->handle(), 0, 1, &m_mesh->vertex_buffer()->handle(), &offset);
        vkCmdBindIndexBuffer(cmd_buf->handle(), m_mesh->index_buffer()->handle(), 0, VK_INDEX_TYPE_UINT32);

        const uint32_t dynamic_offset = m_ubo_size * m_vk_backend->current_frame_idx();

        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_g_buffer_pipeline_layout->handle(), 0, 1, &m_per_frame_ds->handle(), 1, &dynamic_offset);

        for (uint32_t i = 0; i < m_mesh->sub_mesh_count(); i++)
        {
            auto& submesh = m_mesh->sub_meshes()[i];
            auto& mat     = m_mesh->material(submesh.mat_idx);

            if (mat->pbr_descriptor_set())
                vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_g_buffer_pipeline_layout->handle(), 1, 1, &mat->pbr_descriptor_set()->handle(), 0, nullptr);

            // Issue draw call.
            vkCmdDrawIndexed(cmd_buf->handle(), submesh.index_count, 1, submesh.base_index, submesh.base_vertex, 0);
        }

        vkCmdEndRenderPass(cmd_buf->handle());
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void render(dw::vk::CommandBuffer::Ptr cmd_buf)
    {
        DW_SCOPED_SAMPLE("copy", cmd_buf);

        VkClearValue clear_values[2];

        clear_values[0].color.float32[0] = 0.0f;
        clear_values[0].color.float32[1] = 0.0f;
        clear_values[0].color.float32[2] = 0.0f;
        clear_values[0].color.float32[3] = 1.0f;

        clear_values[1].color.float32[0] = 1.0f;
        clear_values[1].color.float32[1] = 1.0f;
        clear_values[1].color.float32[2] = 1.0f;
        clear_values[1].color.float32[3] = 1.0f;

        VkRenderPassBeginInfo info    = {};
        info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass               = m_vk_backend->swapchain_render_pass()->handle();
        info.framebuffer              = m_vk_backend->swapchain_framebuffer()->handle();
        info.renderArea.extent.width  = m_width;
        info.renderArea.extent.height = m_height;
        info.clearValueCount          = 2;
        info.pClearValues             = &clear_values[0];

        vkCmdBeginRenderPass(cmd_buf->handle(), &info, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp;

        vp.x        = 0.0f;
        vp.y        = (float)m_height;
        vp.width    = (float)m_width;
        vp.height   = -(float)m_height;
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;

        vkCmdSetViewport(cmd_buf->handle(), 0, 1, &vp);

        VkRect2D scissor_rect;

        scissor_rect.extent.width  = m_width;
        scissor_rect.extent.height = m_height;
        scissor_rect.offset.x      = 0;
        scissor_rect.offset.y      = 0;

        vkCmdSetScissor(cmd_buf->handle(), 0, 1, &scissor_rect);

        vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferred_pipeline->handle());
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferred_pipeline_layout->handle(), 0, 1, &m_deferred_ds->handle(), 0, nullptr);

        const uint32_t dynamic_offset = m_ubo_size * m_vk_backend->current_frame_idx();

        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferred_pipeline_layout->handle(), 1, 1, &m_per_frame_ds->handle(), 1, &dynamic_offset);

        vkCmdDraw(cmd_buf->handle(), 3, 1, 0, 0);

        render_gui(cmd_buf);

        vkCmdEndRenderPass(cmd_buf->handle());
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void update_uniforms(dw::vk::CommandBuffer::Ptr cmd_buf)
    {
        DW_SCOPED_SAMPLE("update_uniforms", cmd_buf);

        m_transforms.proj_inverse = glm::inverse(m_main_camera->m_projection);
        m_transforms.view_inverse = glm::inverse(m_main_camera->m_view);
        m_transforms.proj         = m_main_camera->m_projection;
        m_transforms.view         = m_main_camera->m_view;
        m_transforms.model        = glm::mat4(1.0f);
        m_transforms.cam_pos      = glm::vec4(m_main_camera->m_position, 0.0f);
        m_transforms.light_dir    = glm::vec4(m_light_direction, 0.0f);

        uint8_t* ptr = (uint8_t*)m_ubo->mapped_ptr();
        memcpy(ptr + m_ubo_size * m_vk_backend->current_frame_idx(), &m_transforms, sizeof(Transforms));
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void update_camera()
    {
        dw::Camera* current = m_main_camera.get();

        float forward_delta = m_heading_speed * m_delta;
        float right_delta   = m_sideways_speed * m_delta;

        current->set_translation_delta(current->m_forward, forward_delta);
        current->set_translation_delta(current->m_right, right_delta);

        m_camera_x = m_mouse_delta_x * m_camera_sensitivity;
        m_camera_y = m_mouse_delta_y * m_camera_sensitivity;

        if (m_mouse_look)
        {
            // Activate Mouse Look
            current->set_rotatation_delta(glm::vec3((float)(m_camera_y),
                                                    (float)(m_camera_x),
                                                    (float)(0.0f)));
        }
        else
        {
            current->set_rotatation_delta(glm::vec3((float)(0),
                                                    (float)(0),
                                                    (float)(0)));
        }

        current->update();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

private:
    // GPU resources.
    size_t m_ubo_size;

    // Common
    dw::vk::DescriptorSet::Ptr       m_per_frame_ds;
    dw::vk::DescriptorSetLayout::Ptr m_per_frame_ds_layout;
    dw::vk::DescriptorSet::Ptr       m_g_buffer_ds;
    dw::vk::DescriptorSetLayout::Ptr m_g_buffer_ds_layout;
    dw::vk::Buffer::Ptr              m_ubo;
    dw::vk::Image::Ptr               m_blue_noise;
    dw::vk::ImageView::Ptr           m_blue_noise_view;

    // Shadow mask pass
    dw::vk::DescriptorSet::Ptr       m_shadow_mask_ds;
    dw::vk::DescriptorSetLayout::Ptr m_shadow_mask_ds_layout;
    dw::vk::RayTracingPipeline::Ptr  m_shadow_mask_pipeline;
    dw::vk::PipelineLayout::Ptr      m_shadow_mask_pipeline_layout;
    dw::vk::Image::Ptr               m_shadow_mask_image;
    dw::vk::ImageView::Ptr           m_shadow_mask_view;
    dw::vk::ShaderBindingTable::Ptr  m_shadow_mask_sbt;

    // Reflection pass
    dw::vk::DescriptorSet::Ptr       m_reflection_ds;
    dw::vk::DescriptorSetLayout::Ptr m_reflection_ds_layout;
    dw::vk::RayTracingPipeline::Ptr  m_reflection_pipeline;
    dw::vk::PipelineLayout::Ptr      m_reflection_pipeline_layout;
    dw::vk::Image::Ptr               m_reflection_image;
    dw::vk::ImageView::Ptr           m_reflection_view;
    dw::vk::ShaderBindingTable::Ptr  m_reflection_sbt;

    // Deferred pass
    dw::vk::GraphicsPipeline::Ptr    m_deferred_pipeline;
    dw::vk::PipelineLayout::Ptr      m_deferred_pipeline_layout;
    dw::vk::DescriptorSet::Ptr       m_deferred_ds;
    dw::vk::DescriptorSetLayout::Ptr m_deferred_layout;

    // G-Buffer pass
    dw::vk::Image::Ptr            m_g_buffer_1; // RGB: Albedo, A: Metallic
    dw::vk::Image::Ptr            m_g_buffer_2; // RGB: Normal, A: Roughness
    dw::vk::Image::Ptr            m_g_buffer_3; // RGB: Position, A: -
    dw::vk::Image::Ptr            m_g_buffer_depth;
    dw::vk::ImageView::Ptr        m_g_buffer_1_view;
    dw::vk::ImageView::Ptr        m_g_buffer_2_view;
    dw::vk::ImageView::Ptr        m_g_buffer_3_view;
    dw::vk::ImageView::Ptr        m_g_buffer_depth_view;
    dw::vk::Framebuffer::Ptr      m_g_buffer_fbo;
    dw::vk::RenderPass::Ptr       m_g_buffer_rp;
    dw::vk::GraphicsPipeline::Ptr m_g_buffer_pipeline;
    dw::vk::PipelineLayout::Ptr   m_g_buffer_pipeline_layout;

    // Camera.
    std::unique_ptr<dw::Camera> m_main_camera;

    // Camera controls.
    bool      m_mouse_look         = false;
    float     m_heading_speed      = 0.0f;
    float     m_sideways_speed     = 0.0f;
    float     m_camera_sensitivity = 0.05f;
    float     m_camera_speed       = 0.05f;
    float     m_offset             = 0.1f;
    bool      m_debug_gui          = true;
    glm::vec3 m_light_direction    = glm::vec3(0.0f);

    // Camera orientation.
    float m_camera_x;
    float m_camera_y;

    // Assets.
    dw::Mesh::Ptr  m_mesh;
    dw::Scene::Ptr m_scene;

    // Uniforms.
    Transforms m_transforms;
};

DW_DECLARE_MAIN(Sample)
