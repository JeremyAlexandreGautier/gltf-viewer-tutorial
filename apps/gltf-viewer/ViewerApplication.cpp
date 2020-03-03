#include "ViewerApplication.hpp"

#include <iostream>
#include <numeric>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include "utils/cameras.hpp"
#include "utils/TrackballCameraController.hpp"
#include "utils/FirstPersonCameraController.hpp"
#include "utils/gltf.hpp"
#include "utils/images.hpp"

#include <stb_image_write.h>

//./bin/gltf-viewer viewer ../Sponza/glTF/Sponza.gltf --lookat "-5.26056,6.59932,0.85661,-4.40144,6.23486,0.497347,0.342113,0.931131,-0.126476"

void keyCallback(
        GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
        glfwSetWindowShouldClose(window, 1);
    }
}

int ViewerApplication::run() {
    // Loader shaders
    const auto glslProgram =
            compileProgram({m_ShadersRootPath / m_AppName / m_vertexShader,
                            m_ShadersRootPath / m_AppName / m_fragmentShader});

    const auto modelViewProjMatrixLocation =
            glGetUniformLocation(glslProgram.glId(), "uModelViewProjMatrix");
    const auto modelViewMatrixLocation =
            glGetUniformLocation(glslProgram.glId(), "uModelViewMatrix");
    const auto normalMatrixLocation =
            glGetUniformLocation(glslProgram.glId(), "uNormalMatrix");

    const auto radianceLocation = glGetUniformLocation(glslProgram.glId(), "uRadiance");
    const auto lightDirlocation = glGetUniformLocation(glslProgram.glId(), "uLightDir");

    glm::vec3 lightDirection = glm::vec3(1);
    glm::vec3 lightIntensity = glm::vec3(1);
    bool cameralight = false;


    tinygltf::Model model;
    if (!loadGltfFile(model)) {
        return -1;
    }

    const auto bufferObject = createBufferObjects(model);
    std::vector<VaoRange> meshToVertexArrays;
    const auto vertexArrayObjects = createVertexArrayObjects(model, bufferObject, meshToVertexArrays);

    glm::vec3 bboxMin;
    glm::vec3 bboxMax;
    computeSceneBounds(model,bboxMin, bboxMax);
    glm::vec3 diagonal_vec = bboxMax - bboxMin;


    const auto maxDistance = glm::distance(bboxMin, bboxMax);

    const auto projMatrix =
            glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
                             0.001f * maxDistance, 1.5f * maxDistance);

    // TODO Implement a new CameraController model and use it instead. Propose the
    // choice from the GUI

    /*FirstPersonCameraController cameraController{
            m_GLFWHandle.window(), 10.f * maxDistance};*/
    //TrackballCameraController cameraController{m_GLFWHandle.window(), 10.f * maxDistance};
    std::unique_ptr<CameraController> cameraController = std::make_unique<TrackballCameraController>(m_GLFWHandle.window(), 0.5f * maxDistance);


    if (m_hasUserCamera) {
        cameraController -> setCamera(m_userCamera);
    } else {
        //cameraController.setCamera(Camera{glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)});
        const auto center_scene = (bboxMax + bboxMin) * 0.5f;
        const auto up = glm::vec3(0,1,0);
        //const auto eye = (bboxMax.z > 0 ) ? bboxMax : center_scene + 2.f * glm::cross(bboxMax, up);
        const auto eye = (diagonal_vec.z > 0 ) ? diagonal_vec : center_scene + 2.f * glm::cross(diagonal_vec, up);

        cameraController -> setCamera(Camera(eye, center_scene, up));
    }


    // Setup OpenGL state for rendering
    glEnable(GL_DEPTH_TEST);
    glslProgram.use();

    // Lambda function to draw the scene
    const auto drawScene = [&](const Camera &camera) {
        glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto viewMatrix = camera.getViewMatrix();
        if(lightDirlocation >= 0){
            if(cameralight){
                glUniform3fv(lightDirlocation, 1, glm::value_ptr(glm::vec3(0, 0, 1)));
            }else {
                glUniform3fv(lightDirlocation, 1,
                             glm::value_ptr(glm::normalize(glm::vec3(viewMatrix * glm::vec4(lightDirection, 0)))));
            }
        }
        if(radianceLocation >= 0){
            glUniform3fv(radianceLocation, 1, glm::value_ptr(lightIntensity));
        }


        // The recursive function that should draw a node
        // We use a std::function because a simple lambda cannot be recursive
        const std::function<void(int, const glm::mat4 &)> drawNode =
                [&](int nodeIdx, const glm::mat4 &parentMatrix) {
                    const auto &node = model.nodes[nodeIdx];
                    glm::mat4 modelMatrix = getLocalToWorldMatrix(node, parentMatrix);
           
                    if (node.mesh >= 0) {
                        glm::mat4 modelViewMatrix = viewMatrix * modelMatrix;
                        glm::mat4 modelViewProjectionMatrix = projMatrix * modelViewMatrix;
                        glm::mat4 normalMatrix = glm::transpose(glm::inverse(modelViewMatrix));
                        glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelViewMatrix));
                        glUniformMatrix4fv(modelViewProjMatrixLocation, 1, GL_FALSE,
                                           glm::value_ptr(modelViewProjectionMatrix));
                        glUniformMatrix4fv(normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));
                        const auto &mesh = model.meshes[node.mesh];
                        const auto &vaoRange = meshToVertexArrays[node.mesh];
                   
                        for (size_t primitiveIdx = 0; primitiveIdx < mesh.primitives.size(); primitiveIdx++) {
                            const auto vaoP = vertexArrayObjects[vaoRange.begin + primitiveIdx];
                            const auto primitive = mesh.primitives[primitiveIdx];
                            glBindVertexArray(vaoP);

                            if (primitive.indices >= 0) {
                  
                                const auto &accessor = model.accessors[primitive.indices];
                                const auto &bufferView = model.bufferViews[accessor.bufferView];
                                const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
                             
                                glDrawElements(primitive.mode, GLsizei(accessor.count), accessor.componentType,
                                               (const GLvoid *) byteOffset);
                            
                            } else {
                       
                                const auto accessorIdx = (*begin(primitive.attributes)).second;
                                const auto &accessor = model.accessors[accessorIdx];
                                glDrawArrays(primitive.mode, 0, GLsizei(accessor.count));
                      
                            }
                        }
               
                    }
                    for (const auto child : node.children) {
                        drawNode(child, modelMatrix);
                    }
                };

        // Draw the scene referenced by gltf file
        if (model.defaultScene >= 0) {
            for (auto node : model.scenes[model.defaultScene].nodes) {
                drawNode(node, glm::mat4(1));
            }
        }
    };


    if(!m_OutputPath.empty()){
        std::vector<unsigned char> pixels(m_nWindowWidth* m_nWindowHeight * 3);
        renderToImage(m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), [&](){drawScene(cameraController -> getCamera());});
        flipImageYAxis(m_nWindowWidth, m_nWindowHeight, 3, pixels.data());
        const auto strPath = m_OutputPath.string();
        stbi_write_png(strPath.c_str(), m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), 0);
        return 0;
    }

    // Loop until the user closes the window
    for (auto iterationCount = 0u; !m_GLFWHandle.shouldClose();
         ++iterationCount) {
        const auto seconds = glfwGetTime();

        const auto camera = cameraController -> getCamera();
        drawScene(camera);

        // GUI code:
        imguiNewFrame();

        {
            ImGui::Begin("GUI");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                        1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("eye: %.3f %.3f %.3f", camera.eye().x, camera.eye().y,
                            camera.eye().z);
                ImGui::Text("diagonal_vec: %.3f %.3f %.3f", camera.center().x,
                            camera.center().y, camera.center().z);
                ImGui::Text(
                        "up: %.3f %.3f %.3f", camera.up().x, camera.up().y, camera.up().z);

                ImGui::Text("front: %.3f %.3f %.3f", camera.front().x, camera.front().y,
                            camera.front().z);
                ImGui::Text("left: %.3f %.3f %.3f", camera.left().x, camera.left().y,
                            camera.left().z);

                if (ImGui::Button("CLI camera args to clipboard")) {
                    std::stringstream ss;
                    ss << "--lookat " << camera.eye().x << "," << camera.eye().y << ","
                       << camera.eye().z << "," << camera.center().x << ","
                       << camera.center().y << "," << camera.center().z << ","
                       << camera.up().x << "," << camera.up().y << "," << camera.up().z;
                    const auto str = ss.str();
                    glfwSetClipboardString(m_GLFWHandle.window(), str.c_str());
                }

                static float theta = 0.f, phi = 0.f;
                ImGui::CollapsingHeader("Angle", ImGuiTreeNodeFlags_DefaultOpen);

                ImGui::SliderFloat("Theta", &theta, 0, 3.14);
                ImGui::SliderFloat("Phi", &phi, 0, 2 * 3.14);

                lightDirection = glm::vec3(glm::sin(theta) * glm::cos(phi),
                        glm::cos(theta),
                        glm::sin(theta) * glm::sin(phi));

                //static glm::vec3 color = glm::vec3(1.0f);
                ImGui::ColorEdit3("Color", (float *)&lightIntensity);
                //lightIntensity = lightIntensity * color;


                static int camera = 0;
                const auto currentCamera = cameraController->getCamera();
                if(ImGui::RadioButton("Trackball", &camera, 0)){
                    cameraController = std::make_unique<TrackballCameraController>(
                            m_GLFWHandle.window(), 0.5f * maxDistance);
                };

                if(ImGui::RadioButton("FirstPerson", &camera, 1)){
                    cameraController = std::make_unique<FirstPersonCameraController>(
                            m_GLFWHandle.window(), 0.5f * maxDistance);
                }

                cameraController->setCamera(currentCamera);

                ImGui::Checkbox("light from camera", &cameralight);

            }
            ImGui::End();
        }

        imguiRenderFrame();

        glfwPollEvents(); // Poll for and process events

        auto ellapsedTime = glfwGetTime() - seconds;
        auto guiHasFocus =
                ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
        if (!guiHasFocus) {
            cameraController -> update(float(ellapsedTime));
        }

        m_GLFWHandle.swapBuffers(); // Swap front and back buffers
    }

    // TODO clean up allocated GL data

    return 0;
}

ViewerApplication::ViewerApplication(const fs::path &appPath, uint32_t width,
                                     uint32_t height, const fs::path &gltfFile,
                                     const std::vector<float> &lookatArgs, const std::string &vertexShader,
                                     const std::string &fragmentShader, const fs::path &output) :
        m_nWindowWidth(width),
        m_nWindowHeight(height),
        m_AppPath{appPath},
        m_AppName{m_AppPath.stem().string()},
        m_ImGuiIniFilename{m_AppName + ".imgui.ini"},
        m_ShadersRootPath{m_AppPath.parent_path() / "shaders"},
        m_gltfFilePath{gltfFile},
        m_OutputPath{output} {
    if (!lookatArgs.empty()) {
        m_hasUserCamera = true;
        m_userCamera =
                Camera{glm::vec3(lookatArgs[0], lookatArgs[1], lookatArgs[2]),
                       glm::vec3(lookatArgs[3], lookatArgs[4], lookatArgs[5]),
                       glm::vec3(lookatArgs[6], lookatArgs[7], lookatArgs[8])};
    }

    if (!vertexShader.empty()) {
        m_vertexShader = vertexShader;
    }

    if (!fragmentShader.empty()) {
        m_fragmentShader = fragmentShader;
    }

    ImGui::GetIO().IniFilename =
            m_ImGuiIniFilename.c_str(); // At exit, ImGUI will store its windows
    // positions in this file

    glfwSetKeyCallback(m_GLFWHandle.window(), keyCallback);

    printGLVersion();
}

bool ViewerApplication::loadGltfFile(tinygltf::Model &model) {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_gltfFilePath.string());

    if (!warn.empty()) {
        std::cerr << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << err << std::endl;
    }

    if (!ret) {
        std::cerr << "Failed to parse glTF file" << std::endl;
        return false;
    }
    return true;
}

std::vector<GLuint> ViewerApplication::createBufferObjects(const tinygltf::Model &model) {
    std::vector<GLuint> bufferObjects(model.buffers.size(), 0);
    glGenBuffers(model.buffers.size(), bufferObjects.data());


    for (size_t i = 0; i < model.buffers.size(); i++) {
        glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[i]);
        glBufferStorage(GL_ARRAY_BUFFER, model.buffers[i].data.size(), model.buffers[i].data.data(), 0); //Not available with opengl 4.1
        //glBufferData(GL_ARRAY_BUFFER, model.buffers[i].data.size(), model.buffers[i].data.data(), 0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return bufferObjects;
}

std::vector<GLuint>
ViewerApplication::createVertexArrayObjects(const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects,
                                            std::vector<VaoRange> &meshToVertexArrays) {

    const std::vector<std::string> ATTRIB_NAMES = {"POSITION", "NORMAL", "TEXCOORD_0"};
    const std::unordered_map<std::string, GLuint> VERTEX_ATTRIBS = {
            {"POSITION",   0},
            {"NORMAL",     1},
            {"TEXCOORD_0", 2}
    };

    std::vector<GLuint> vertexArrayObjects;
    meshToVertexArrays.resize(model.meshes.size());

    for (size_t meshIdx = 0; meshIdx < model.meshes.size(); meshIdx++) {
        const auto &mesh = model.meshes[meshIdx];
        auto &vaoRange = meshToVertexArrays[meshIdx];

        vaoRange.begin = GLsizei(vertexArrayObjects.size());
        vaoRange.count = GLsizei(mesh.primitives.size());

        vertexArrayObjects.resize(vaoRange.begin + mesh.primitives.size());

        glGenVertexArrays(vaoRange.count, &vertexArrayObjects[vaoRange.begin]);

        const auto primitiveSize = mesh.primitives.size();
        for (size_t primitiveIdx = 0; primitiveIdx < primitiveSize; primitiveIdx++) {
            auto primitive = mesh.primitives[primitiveIdx];
            glBindVertexArray(vertexArrayObjects[vaoRange.begin + primitiveIdx]);
            /* _____________________________________________________________________________ */
            for (int i = 0; i < primitive.attributes.size() - 1; i++) {
                // I'm opening a scope because I want to reuse the variable iterator in the code for NORMAL and TEXCOORD_0
                const auto iterator = primitive.attributes.find(ATTRIB_NAMES[i]);
                if (iterator != end(primitive.attributes)) { // If "ATTRIB_NAMES[i]" has been found in the map
                    // (*iterator).first is the key "POSITION", (*iterator).second is the value, ie. the index of the accessor for this attribute
                    const auto accessorIdx = (*iterator).second;
                    const auto &accessor = model.accessors[accessorIdx];
                    const auto &bufferView = model.bufferViews[accessor.bufferView];
                    const auto bufferIdx = bufferView.buffer;

                    const auto bufferObject = model.buffers[bufferIdx];

                    glEnableVertexAttribArray(VERTEX_ATTRIBS.at(ATTRIB_NAMES[i]));
                    glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[bufferIdx]);
                    const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
                    glVertexAttribPointer(
                            GLuint(VERTEX_ATTRIBS.at(ATTRIB_NAMES[i])),
                            accessor.type,
                            accessor.componentType,
                            GL_FALSE,
                            GLsizei(bufferView.byteStride),
                            (GLvoid *) (byteOffset)
                    );
                }
            }
            /* _____________________________________________________________________________ */
            if (primitive.indices >= 0) {
                const auto &accessor = model.accessors[primitive.indices];
                const auto &bufferView = model.bufferViews[accessor.bufferView];
                const auto bufferIdx = bufferView.buffer;
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObjects[bufferIdx]);
            }
        }
        glBindVertexArray(0);
    }
    return vertexArrayObjects;
}





