#include <string>

const std::string SCENE_PATH = "../../pbrt-v4-scenes/head/head.pbrt";
int main(int argc, char *argv[]) {
    
    // Initialize the ray tracer
    init();

    // Parse scene file
    Scene scene;
    SceneBuilder builder(&scene);
    parseFile(&builder, SCENE_PATH)
    
    // Render the scene
    renderCPU(scene);

    // Clean up
    cleanup();
    return 0;
}
