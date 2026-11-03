#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>
#include <link.h>
#include "dobby.h"

#define LOG_TAG "SnakeModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
using DomainGet = void* (*)();
using DomainGetAssemblies = const void** (*)(void*, size_t*);
using AssemblyGetImage = const void* (*)(const void*);
using ImageGetName = const char* (*)(const void*);
using ClassFromName = void* (*)(const void*, const char*, const char*);
using ClassGetMethod = const void* (*)(void*, const char*, int);
using ObjectGetClass = void* (*)(void*);
using ClassGetName = const char* (*)(void*);
using ClassGetNamespace = const char* (*)(void*);
using ThreadAttach = void* (*)(void*);
using ThreadDetach = void (*)(void*);

std::atomic<float> g_multiplier{1.0f};
std::atomic<bool> g_hooked{false};
JavaVM* g_vm = nullptr;

DomainGet il2cpp_domain_get = nullptr;
DomainGetAssemblies il2cpp_domain_get_assemblies = nullptr;
AssemblyGetImage il2cpp_assembly_get_image = nullptr;
ImageGetName il2cpp_image_get_name = nullptr;
ClassFromName il2cpp_class_from_name = nullptr;
ClassGetMethod il2cpp_class_get_method_from_name = nullptr;
ObjectGetClass il2cpp_object_get_class = nullptr;
ClassGetName il2cpp_class_get_name = nullptr;
ClassGetNamespace il2cpp_class_get_namespace = nullptr;
ThreadAttach il2cpp_thread_attach = nullptr;
ThreadDetach il2cpp_thread_detach = nullptr;

using SpeedGetter = float (*)(void*, const void*);
SpeedGetter original_get_speed = nullptr;

float hooked_get_speed(void* self, const void* method) {
    const float original = original_get_speed ? original_get_speed(self, method) : 0.0f;
    if (!self || !il2cpp_object_get_class || !il2cpp_class_get_name) return original;

    void* klass = il2cpp_object_get_class(self);
    if (!klass) return original;
    const char* name = il2cpp_class_get_name(klass);
    const char* ns = il2cpp_class_get_namespace ? il2cpp_class_get_namespace(klass) : "";
    // Only change the locally controlled player snake; leave AI/other snakes untouched.
    if (!name || std::strcmp(name, "PlayerSnakeController") != 0 ||
        !ns || std::strcmp(ns, "SnakeIO") != 0) {
        return original;
    }
    return original * g_multiplier.load(std::memory_order_relaxed);
}

bool resolve_il2cpp() {
    void* handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
    if (!handle) handle = dlopen("libil2cpp.so", RTLD_NOW);
    if (!handle) return false;
#define RESOLVE(name) name = reinterpret_cast<decltype(name)>(dlsym(handle, #name))
    RESOLVE(il2cpp_domain_get);
    RESOLVE(il2cpp_domain_get_assemblies);
    RESOLVE(il2cpp_assembly_get_image);
    RESOLVE(il2cpp_image_get_name);
    RESOLVE(il2cpp_class_from_name);
    RESOLVE(il2cpp_class_get_method_from_name);
    RESOLVE(il2cpp_object_get_class);
    RESOLVE(il2cpp_class_get_name);
    RESOLVE(il2cpp_class_get_namespace);
    RESOLVE(il2cpp_thread_attach);
    RESOLVE(il2cpp_thread_detach);
#undef RESOLVE
    return il2cpp_domain_get && il2cpp_domain_get_assemblies &&
           il2cpp_assembly_get_image && il2cpp_image_get_name &&
           il2cpp_class_from_name && il2cpp_class_get_method_from_name &&
           il2cpp_object_get_class && il2cpp_class_get_name;
}

JNIEnv* get_env(bool& attached) {
    attached = false;
    if (!g_vm) {
        void* art = dlopen("libart.so", RTLD_NOW);
        if (art) {
            using GetCreatedVMs = jint (*)(JavaVM**, jsize, jsize*);
            auto get_vms = reinterpret_cast<GetCreatedVMs>(dlsym(art, "JNI_GetCreatedJavaVMs"));
            if (get_vms) {
                JavaVM* vm = nullptr;
                jsize count = 0;
                if (get_vms(&vm, 1, &count) == JNI_OK && count > 0) g_vm = vm;
            }
        }
    }
    if (!g_vm) return nullptr;
    JNIEnv* env = nullptr;
    jint status = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) return nullptr;
        attached = true;
    } else if (status != JNI_OK) {
        return nullptr;
    }
    return env;
}

void start_java_overlay() {
    bool attached = false;
    JNIEnv* env = get_env(attached);
    if (!env) return;

    jclass activity_thread = env->FindClass("android/app/ActivityThread");
    if (!activity_thread) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (attached) g_vm->DetachCurrentThread();
        return;
    }
    jmethodID current_app = env->GetStaticMethodID(activity_thread, "currentApplication", "()Landroid/app/Application;");
    if (!current_app) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (attached) g_vm->DetachCurrentThread();
        return;
    }
    jobject app = env->CallStaticObjectMethod(activity_thread, current_app);
    if (!app || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (attached) g_vm->DetachCurrentThread();
        return;
    }

    jclass app_class = env->GetObjectClass(app);
    jmethodID get_loader = env->GetMethodID(app_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = get_loader ? env->CallObjectMethod(app, get_loader) : nullptr;
    if (!loader || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (attached) g_vm->DetachCurrentThread();
        return;
    }
    jclass loader_class = env->GetObjectClass(loader);
    jmethodID load_class = env->GetMethodID(loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring helper_name = env->NewStringUTF("com.snakemod.OverlayBootstrap");
    jclass helper = load_class ? static_cast<jclass>(env->CallObjectMethod(loader, load_class, helper_name)) : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        helper = nullptr;
    }
    if (helper) {
        jmethodID start = env->GetStaticMethodID(helper, "start", "(Landroid/content/Context;)V");
        if (start) env->CallStaticVoidMethod(helper, start, app);
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGI("overlay bootstrap requested");
    } else {
        LOGW("could not load Java overlay bootstrap");
    }
    if (attached) g_vm->DetachCurrentThread();
}

void* worker(void*) {
    LOGI("native module loaded; waiting for IL2CPP");
    for (int i = 0; i < 900; ++i) {
        if (resolve_il2cpp()) break;
        usleep(200000);
    }
    if (!resolve_il2cpp()) {
        LOGE("IL2CPP exports not found; hook not installed");
        return nullptr;
    }
    void* domain = il2cpp_domain_get();
    for (int i = 0; !domain && i < 300; ++i) {
        usleep(200000);
        domain = il2cpp_domain_get();
    }
    if (!domain) { LOGE("IL2CPP domain unavailable"); return nullptr; }
    void* il2cpp_thread = il2cpp_thread_attach ? il2cpp_thread_attach(domain) : nullptr;

    size_t count = 0;
    const void** assemblies = il2cpp_domain_get_assemblies(domain, &count);
    const void* image = nullptr;
    for (size_t i = 0; assemblies && i < count; ++i) {
        const void* candidate = il2cpp_assembly_get_image(assemblies[i]);
        const char* name = candidate ? il2cpp_image_get_name(candidate) : nullptr;
        if (name && std::strcmp(name, "Assembly-CSharp.dll") == 0) { image = candidate; break; }
    }
    if (!image) { LOGE("Assembly-CSharp.dll not found"); return nullptr; }
    void* snake_controller = il2cpp_class_from_name(image, "SnakeIO", "SnakeController");
    const void* method = snake_controller ? il2cpp_class_get_method_from_name(snake_controller, "get_Speed", 0) : nullptr;
    void* target = method ? *reinterpret_cast<void* const*>(method) : nullptr;
    if (!target) { LOGE("SnakeController.get_Speed method pointer missing"); return nullptr; }
    int result = DobbyHook(target, reinterpret_cast<void*>(hooked_get_speed), reinterpret_cast<void**>(&original_get_speed));
    if (result == 0 && original_get_speed) {
        g_hooked.store(true);
        LOGI("speed hook installed at %p", target);
    } else {
        LOGE("DobbyHook failed (%d)", result);
    }
    if (il2cpp_thread && il2cpp_thread_detach) il2cpp_thread_detach(il2cpp_thread);
    // Show the floating controls even if the hook fails so the user sees a diagnostic.
    start_java_overlay();
    return nullptr;
}

__attribute__((constructor)) void module_init() {
    pthread_t thread;
    if (pthread_create(&thread, nullptr, worker, nullptr) == 0) pthread_detach(thread);
}
} // namespace

extern "C" __attribute__((visibility("default"))) JNIEXPORT void JNICALL
Java_com_snakemod_OverlayService_nativeSetMultiplier(JNIEnv*, jclass, jfloat value) {
    if (value < 1.0f) value = 1.0f;
    if (value > 5.0f) value = 5.0f;
    g_multiplier.store(value, std::memory_order_relaxed);
}
