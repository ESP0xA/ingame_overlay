#pragma once

#include <future>
#include <chrono>
#include <memory>

#include "Renderer_Hook.h"

std::future<Renderer_Hook*> detect_renderer(std::chrono::milliseconds timeout = std::chrono::milliseconds{-1});
