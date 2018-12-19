#include <vsg/all.h>

#include <vsg/viewer/Camera.h>
#include <vsg/ui/PrintEvents.h>

#include <iostream>
#include <chrono>

class EscapeSetsDone : public vsg::Visitor
{
public:
        bool done = false;

        void apply(vsg::KeyPressEvent& keyPress) override
        {
            if (keyPress.keyBase==vsg::KEY_Escape) done = true;
        }
};

class CameraManipulator : public vsg::Visitor
{
public:
    virtual bool update(vsg::Camera& camera) = 0;
};

class Trackball : public CameraManipulator
{
public:

        Trackball(vsg::Camera* camera=nullptr) :
            _camera(camera) {}

        /// compute non dimensional window coordinate (-1,1) from event coords
        vsg::dvec2 ndc(vsg::PointerEvent& event)
        {
            vsg::dvec2 v = vsg::dvec2((static_cast<double>(event.x)/window_width)*2.0-1.0, (static_cast<double>(event.y)/window_height)*2.0-1.0);
            std::cout<<"ndc = "<<v<<std::endl;
            return v;
        }

        /// compute trackball coordinate from event coords
        vsg::dvec3 tbc(vsg::PointerEvent& event)
        {
            vsg::dvec2 v = ndc(event);

            double l = length(v);
            std::cout<<"tdc (), nfc = "<<v<<", l="<<l<<std::endl;
            if (l<1.0f)
            {
                double h = sqrt(1.0-l*l);
                return vsg::dvec3(v.x, v.y, h);
            }
            else
            {
                return vsg::dvec3(v.x, v.y, 0.0);
            }
        }

        void apply(vsg::ExposeWindowEvent& exposeWindow) override
        {
            std::cout<<"Expose "<<exposeWindow.width<<", "<<exposeWindow.height<<std::endl;
            window_width = static_cast<double>(exposeWindow.width);
            window_height = static_cast<double>(exposeWindow.height);
        }

        void apply(vsg::ButtonPressEvent& buttonPress) override
        {
            prev_ndc = ndc(buttonPress);
            prev_tbc = tbc(buttonPress);
            prev_time = buttonPress.time;
            if (buttonPress.mask & vsg::BUTTON_MASK_4)
            {
                std::cout<<"buttonPress :  Mouse Wheel Up button="<<buttonPress.button<<std::endl;
            }
            else if (buttonPress.mask & vsg::BUTTON_MASK_5)
            {
                std::cout<<"buttonPress : Mouse Wheel Down button="<<buttonPress.button<<std::endl;
            }
            else
            {
                std::cout<<"buttonPress : button = "<<buttonPress.mask<<" button="<<buttonPress.button<<std::endl;
            }
        };

        void apply(vsg::ButtonReleaseEvent& buttonRelease) override
        {
            prev_ndc = ndc(buttonRelease);
            prev_tbc = tbc(buttonRelease);
            prev_time = buttonRelease.time;
            if (buttonRelease.mask & vsg::BUTTON_MASK_4)
            {
                std::cout<<"buttonRelease :  Mouse Wheel Up, button="<<buttonRelease.button<<std::endl;
            }
            else if (buttonRelease.mask & vsg::BUTTON_MASK_5)
            {
                std::cout<<"buttonRelease : Mouse Wheel Down button="<<buttonRelease.button<<std::endl;
            }
            else
            {
                std::cout<<"buttonRelease : button = "<<buttonRelease.mask<<std::endl;
            }
        };

        void apply(vsg::MoveEvent& moveEvent) override
        {
            vsg::dvec2 new_ndc = ndc(moveEvent);
            vsg::dvec3 new_tbc = tbc(moveEvent);
            vsg::time_point new_time = moveEvent.time;
            double delta_time = std::chrono::duration<double, std::chrono::seconds::period>(new_time-prev_time).count();

            std::cout<<"Move tbc=("<<new_tbc<<")"<<std::endl;

            if (moveEvent.mask & vsg::BUTTON_MASK_1)
            {
                vsg::dvec2 delta = new_ndc - prev_ndc;
                vsg::dvec3 xp = vsg::cross(vsg::normalize(prev_tbc), vsg::normalize(new_tbc));
                double xp_len = vsg::length(xp);
                if (xp_len>0.0)
                {
                    vsg::dvec3 axis = xp/xp_len;
                    double angle = asin(xp_len);
                    vsg::dmat4 rotation = vsg::rotate(angle, axis);
                    //rotation = vsg::rotate(angle, 0.0, 0.0, 1.0);


                    //accumulated_transform = accumulated_transform * rotation;
                    accumulated_transform = rotation;

                    std::cout<<"    Rotate axis=("<<axis<<") angle="<<vsg::degrees(angle)<<" rotation_spreed="<<vsg::degrees(angle)/delta_time<<std::endl;
                    std::cout<<"    rotation = ("<<rotation<<")"<<std::endl;

                    update();
                }
                else
                {
                    std::cout<<"    No rotation=("<<xp<<")"<<std::endl;
                }
            }
            else if (moveEvent.mask & vsg::BUTTON_MASK_2)
            {
                vsg::dvec2 delta = new_ndc - prev_ndc;
                std::cout<<"   Zoom "<<delta<<std::endl;
            }
            else if (moveEvent.mask & vsg::BUTTON_MASK_3)
            {
                vsg::dvec2 delta = new_ndc - prev_ndc;
                std::cout<<"   Pan "<<delta<<std::endl;
            }
            else if (moveEvent.mask & vsg::BUTTON_MASK_4)
            {
                std::cout<<"   Mouse Wheel Up "<<std::endl;
            }
            else if (moveEvent.mask & vsg::BUTTON_MASK_5)
            {
                std::cout<<"   Mouse Wheel Down "<<std::endl;
            }
            else
            {
                std::cout<<"   MoveEvent "<<moveEvent.mask<<std::endl;
            }
            prev_ndc = new_ndc;
            prev_tbc = new_tbc;
            prev_time = new_time;
        };

        void transform(vsg::LookAt* lookAt)
        {
            std::cout<<"transform() = "<<accumulated_transform<<std::endl;

            auto center = lookAt->center;
#if 1
            vsg::dmat4 matrix = vsg::translate(center) * accumulated_transform * vsg::translate(-center);
#else
            vsg::dmat4 matrix = vsg::translate(-center) * accumulated_transform * vsg::translate(center);
#endif

            lookAt->up = vsg::normalize(matrix * (center+lookAt->up) - matrix * center);
            lookAt->center = matrix * center;
            lookAt->eye = matrix * lookAt->eye;

            std::cout<<"    lookAt->eye = "<<lookAt->eye<<std::endl;
            std::cout<<"    lookAt->center = "<<lookAt->center<<std::endl;
            std::cout<<"    lookAt->up = "<<lookAt->up<<std::endl;

            accumulated_transform = vsg::dmat4(); // reset to identity
        }

        bool update(vsg::Camera& camera) override
        {
            vsg::LookAt* lookAt = dynamic_cast<vsg::LookAt*>(camera.getViewMatrix());
            if (lookAt) transform(lookAt);

            return false;
        }

        bool update()
        {
            return _camera ? update(*_camera) : false;
        }

        vsg::ref_ptr<vsg::Camera> _camera;

        double window_width = 800.0;
        double window_height = 600.0;
        vsg::dvec2 prev_ndc;
        vsg::dvec3 prev_tbc;
        vsg::time_point prev_time;

        vsg::dmat4 accumulated_transform;
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto numFrames = arguments.value(-1, "-f");
    auto printFrameRate = arguments.read("--fr");
    auto numWindows = arguments.value(1, "--num-windows");
    auto textureFile = arguments.value(std::string("textures/lz.vsgb"), "-t");
    auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(800, 600), {"--window", "-w"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    vsg::ref_ptr<vsg::Shader> vertexShader = vsg::Shader::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::Shader> fragmentShader = vsg::Shader::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return 1;
    }

    vsg::vsgReaderWriter vsgReader;
    auto textureData = vsgReader.read<vsg::Data>(vsg::findFile(textureFile, searchPaths));
    if (!textureData)
    {
        std::cout<<"Could not read texture file : "<<textureFile<<std::endl;
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(width, height, debugLayer, apiDumpLayer));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    for(int i=1; i<numWindows; ++i)
    {
        vsg::ref_ptr<vsg::Window> new_window(vsg::Window::create(width, height, debugLayer, apiDumpLayer, window));
        viewer->addWindow( new_window );
    }

    // create high level Vulkan objects associated the main window
    vsg::ref_ptr<vsg::PhysicalDevice> physicalDevice(window->physicalDevice());
    vsg::ref_ptr<vsg::Device> device(window->device());
    vsg::ref_ptr<vsg::Surface> surface(window->surface());
    vsg::ref_ptr<vsg::RenderPass> renderPass(window->renderPass());


    VkQueue graphicsQueue = device->getQueue(physicalDevice->getGraphicsFamily());
    VkQueue presentQueue = device->getQueue(physicalDevice->getPresentFamily());
    if (!graphicsQueue || !presentQueue)
    {
        std::cout<<"Required graphics/present queue not available!"<<std::endl;
        return 1;
    }

    vsg::ref_ptr<vsg::CommandPool> commandPool = vsg::CommandPool::create(device, physicalDevice->getGraphicsFamily());

    // set up vertex and index arrays
    vsg::ref_ptr<vsg::vec3Array> vertices(new vsg::vec3Array
    {
        {-0.5f, -0.5f, 0.0f},
        {0.5f,  -0.5f, 0.05f},
        {0.5f , 0.5f, 0.0f},
        {-0.5f, 0.5f, 0.0f},
        {-0.5f, -0.5f, -0.5f},
        {0.5f,  -0.5f, -0.5f},
        {0.5f , 0.5f, -0.5},
        {-0.5f, 0.5f, -0.5}
    });

    vsg::ref_ptr<vsg::vec3Array> colors(new vsg::vec3Array
    {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
    });

    vsg::ref_ptr<vsg::vec2Array> texcoords(new vsg::vec2Array
    {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    });

    vsg::ref_ptr<vsg::ushortArray> indices(new vsg::ushortArray
    {
        0, 1, 2,
        2, 3, 0,
        4, 5, 6,
        6, 7, 4
    });

    auto vertexBufferData = vsg::createBufferAndTransferData(device, commandPool, graphicsQueue, vsg::DataList{vertices, colors, texcoords}, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    auto indexBufferData = vsg::createBufferAndTransferData(device, commandPool, graphicsQueue, {indices}, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);


    //
    // set up texture image
    //
    vsg::ImageData imageData = vsg::transferImageData(device, commandPool, graphicsQueue, textureData);
    if (!imageData.valid())
    {
        std::cout<<"Texture not created"<<std::endl;
        return 1;
    }

    vsg::ref_ptr<vsg::mat4Value> projMatrix(new vsg::mat4Value);
    vsg::ref_ptr<vsg::mat4Value> viewMatrix(new vsg::mat4Value);
    vsg::ref_ptr<vsg::mat4Value> modelMatrix(new vsg::mat4Value);

    //
    // set up descriptor layout and descriptor set and pieline layout for uniforms
    //
    vsg::ref_ptr<vsg::DescriptorPool> descriptorPool = vsg::DescriptorPool::create(device, 1,
    {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    });

    vsg::ref_ptr<vsg::PushConstants> pushConstant_proj = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 0, projMatrix);
    vsg::ref_ptr<vsg::PushConstants> pushConstant_view = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 64, viewMatrix);
    vsg::ref_ptr<vsg::PushConstants> pushConstant_model = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 128, modelMatrix);

    vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout = vsg::DescriptorSetLayout::create(device,
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    });

    vsg::PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196}
    };

    vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(device, descriptorPool, descriptorSetLayout,
    {
        vsg::DescriptorImage::create(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vsg::ImageDataList{imageData})
    });

    vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout = vsg::PipelineLayout::create(device, {descriptorSetLayout}, pushConstantRanges);


    // setup binding of descriptors
    vsg::ref_ptr<vsg::BindDescriptorSets> bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, vsg::DescriptorSets{descriptorSet}); // device dependent


    // set up graphics pipeline
    vsg::VertexInputState::Bindings vertexBindingsDescriptions
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX},
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX},
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0},
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},
    };

    vsg::ref_ptr<vsg::ShaderStages> shaderStages = vsg::ShaderStages::create(vsg::ShaderModules
    {
        vsg::ShaderModule::create(device, vertexShader),
        vsg::ShaderModule::create(device, fragmentShader)
    });

    auto viewport = vsg::ViewportState::create(VkExtent2D{width, height});

    vsg::ref_ptr<vsg::GraphicsPipeline> pipeline = vsg::GraphicsPipeline::create(device, renderPass, pipelineLayout, // device dependent
    {
        shaderStages,  // device dependent
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),// device independent
        vsg::InputAssemblyState::create(), // device independent
        viewport, // device independent
        vsg::RasterizationState::create(),// device independent
        vsg::MultisampleState::create(),// device independent
        vsg::ColorBlendState::create(),// device independent
        vsg::DepthStencilState::create()// device independent
    });

    vsg::ref_ptr<vsg::BindPipeline> bindPipeline = vsg::BindPipeline::create(pipeline);

    // set up vertex buffer binding
    vsg::ref_ptr<vsg::BindVertexBuffers> bindVertexBuffers = vsg::BindVertexBuffers::create(0, vertexBufferData);  // device dependent

    // set up index buffer binding
    vsg::ref_ptr<vsg::BindIndexBuffer> bindIndexBuffer = vsg::BindIndexBuffer::create(indexBufferData.front(), VK_INDEX_TYPE_UINT16); // device dependent

    // set up drawing of the triangles
    vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0); // device independent

    // set up what we want to render in a command graph
    // create command graph to contain all the Vulkan calls for specifically rendering the model
    vsg::ref_ptr<vsg::StateGroup> commandGraph = vsg::StateGroup::create();

    // set up the state configuration
    commandGraph->add(bindPipeline);  // device dependent
    commandGraph->add(bindDescriptorSets);  // device dependent

    commandGraph->add(pushConstant_proj);
    commandGraph->add(pushConstant_view);
    commandGraph->add(pushConstant_model);

    // add subgraph that represents the model to render
    vsg::ref_ptr<vsg::StateGroup> model = vsg::StateGroup::create();
    commandGraph->addChild(model);

    // add the vertex and index buffer data
    model->add(bindVertexBuffers); // device dependent
    model->add(bindIndexBuffer); // device dependent

    // add the draw primitive command
    model->addChild(drawIndexed); // device independent

    //
    // end of initialize vulkan
    //
    /////////////////////////////////////////////////////////////////////

    auto startTime =std::chrono::steady_clock::now();

    for (auto& win : viewer->windows())
    {
        // add a GraphicsStage tp the Window to do dispatch of the command graph to the commnad buffer(s)
        win->addStage(vsg::GraphicsStage::create(commandGraph));
        win->populateCommandBuffers();
    }


    vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 10.0));
    vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(vsg::dvec3(1.0, 1.0, 0.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt));

    bool windowResized = false;
    float time = 0.0f;
    vsg::ref_ptr<Trackball> trackball(new Trackball(camera));
    vsg::ref_ptr<EscapeSetsDone> escapeSetsDone(new EscapeSetsDone);

    using EventHandlers = std::list<vsg::ref_ptr<vsg::Visitor>>;
    EventHandlers eventHandlers{trackball, escapeSetsDone};

    while (!viewer->done() && !escapeSetsDone->done && (numFrames<0 || (numFrames--)>0))
    {
        if (viewer->pollEvents())
        {
            for (auto& vsg_event : viewer->getEvents())
            {
                for(auto& handler : eventHandlers)
                {
                    vsg_event->accept(*handler);
                }
            }
        }

        float previousTime = time;
        time = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::steady_clock::now()-viewer->start_point()).count();
        if (printFrameRate) std::cout<<"time = "<<time<<" fps="<<1.0/(time-previousTime)<<std::endl;



        camera->getProjectionMatrix()->get((*projMatrix));
        camera->getViewMatrix()->get((*viewMatrix));

        //(*projMatrix) = vsg::perspective(vsg::radians(45.0f), float(width)/float(height), 0.1f, 10.f);
        //(*viewMatrix) = vsg::lookAt(vsg::vec3(2.0f, 2.0f, 2.0f), vsg::vec3(0.0f, 0.0f, 0.0f), vsg::vec3(0.0f, 0.0f, 1.0f));

        //(*modelMatrix) = vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f));

        for (auto& win : viewer->windows())
        {
            // we need to regenerate the CommandBuffer so that the PushConstants get called with the new values.
            win->populateCommandBuffers();
        }

        if (window->resized()) windowResized = true;

        viewer->submitFrame();

        if (windowResized)
        {
            windowResized = false;

            auto windowExtent = window->extent2D();
            viewport->getViewport().width = static_cast<float>(windowExtent.width);
            viewport->getViewport().height = static_cast<float>(windowExtent.height);
            viewport->getScissor().extent = windowExtent;

            vsg::ref_ptr<vsg::GraphicsPipeline> new_pipeline = vsg::GraphicsPipeline::create(device, renderPass, pipelineLayout, pipeline->getPipelineStates());

            bindPipeline->setPipeline(new_pipeline);

            pipeline = new_pipeline;

            perspective->aspectRatio = static_cast<double>(windowExtent.width) / static_cast<double>(windowExtent.height);

            std::cout<<"window aspect ratio = "<<perspective->aspectRatio<<std::endl;
        }
    }


    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
