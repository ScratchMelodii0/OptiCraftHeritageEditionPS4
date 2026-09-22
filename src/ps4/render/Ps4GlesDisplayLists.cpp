#ifdef PS4_PLATFORM

#include "ps4/render/Ps4GlesDisplayLists.h"
#include "ps4/render/Ps4GlesPipeline.h"
#include "ps4/system/Ps4DebugLog.h"

#include "ps4/render/Ps4Gl.h"

#include <cstring>
#include <unordered_map>
#include <vector>

namespace Ps4Gles
{
namespace DisplayLists
{
namespace
{
struct DrawBatch
{
    std::size_t offset = 0;        // into the list VBO
    RenderInterleavedMesh layout;  // data == nullptr, first == 0
};

struct Command
{
    // Exactly one of these is used: a replayed RenderAPI call, or a batch.
    std::function<void()> call;
    int batch = -1;
};

struct List
{
    GLuint vbo = 0;
    std::size_t vboBytes = 0;
    std::vector<DrawBatch> batches;
    std::vector<Command> commands;
};

std::unordered_map<int, List> s_lists;
int s_nextId = 1;

int s_recordingId = 0;
List* s_recording = nullptr;
std::vector<unsigned char> s_staging;

// glCallList inside a list is legal and recursion is bounded by GL at 64.
int s_callDepth = 0;
constexpr int kMaxCallDepth = 64;

void releaseStorage(List& list)
{
    if (list.vbo != 0)
        glDeleteBuffers(1, &list.vbo);
    list = List{};
}
}

int generate(int count)
{
    if (count <= 0)
        return 0;
    const int first = s_nextId;
    s_nextId += count;
    for (int i = 0; i < count; ++i)
        s_lists.emplace(first + i, List{});
    return first;
}

void remove(int first, int count)
{
    for (int i = 0; i < count; ++i)
    {
        auto it = s_lists.find(first + i);
        if (it == s_lists.end())
            continue;
        if (s_recording == &it->second)
        {
            s_recording = nullptr;
            s_recordingId = 0;
        }
        releaseStorage(it->second);
        s_lists.erase(it);
    }
}

void begin(int list)
{
    if (s_recording != nullptr)
    {
        Ps4DebugLog::printf("[ps4-gles] glNewList(%d) while %d is open; ignored\n", list, s_recordingId);
        return;
    }
    List& target = s_lists[list];   // GL allows compiling into an ungenerated name
    releaseStorage(target);
    s_recording = &target;
    s_recordingId = list;
    s_staging.clear();
}

void end()
{
    if (s_recording == nullptr)
        return;
    List& list = *s_recording;
    if (!s_staging.empty())
    {
        glGenBuffers(1, &list.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, list.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(s_staging.size()), s_staging.data(), GL_STATIC_DRAW);
        list.vboBytes = s_staging.size();
    }
    s_staging.clear();
    s_staging.shrink_to_fit();
    s_recording = nullptr;
    s_recordingId = 0;
}

bool recording()
{
    return s_recording != nullptr;
}

void record(std::function<void()> command)
{
    if (s_recording == nullptr)
        return;
    Command c;
    c.call = std::move(command);
    s_recording->commands.push_back(std::move(c));
}

bool recordDraw(const RenderInterleavedMesh& mesh)
{
    if (s_recording == nullptr || mesh.data == nullptr || mesh.stride <= 0 || mesh.count <= 0)
        return false;

    const std::size_t bytes = static_cast<std::size_t>(mesh.count) * static_cast<std::size_t>(mesh.stride);
    const unsigned char* source = static_cast<const unsigned char*>(mesh.data) +
                                  static_cast<std::size_t>(mesh.first) * static_cast<std::size_t>(mesh.stride);

    DrawBatch batch;
    batch.offset = (s_staging.size() + 15u) & ~static_cast<std::size_t>(15u);
    batch.layout = mesh;
    batch.layout.data = nullptr;
    batch.layout.first = 0;
    s_staging.resize(batch.offset + bytes);
    std::memcpy(s_staging.data() + batch.offset, source, bytes);

    Command c;
    c.batch = static_cast<int>(s_recording->batches.size());
    s_recording->batches.push_back(batch);
    s_recording->commands.push_back(std::move(c));
    return true;
}

void call(int id)
{
    if (s_recording != nullptr)
    {
        record([id]() { call(id); });
        return;
    }
    if (s_callDepth >= kMaxCallDepth)
        return;
    auto it = s_lists.find(id);
    if (it == s_lists.end())
        return;

    ++s_callDepth;
    const List& list = it->second;
    for (const Command& command : list.commands)
    {
        if (command.batch >= 0)
        {
            const DrawBatch& batch = list.batches[static_cast<std::size_t>(command.batch)];
            drawFromBuffer(list.vbo, batch.offset, batch.layout);
        }
        else if (command.call)
        {
            command.call();
        }
    }
    --s_callDepth;
}

void callMany(int count, const int* lists)
{
    if (lists == nullptr)
        return;
    for (int i = 0; i < count; ++i)
        call(lists[i]);
}

std::size_t residentBytes()
{
    std::size_t total = 0;
    for (const auto& entry : s_lists)
        total += entry.second.vboBytes;
    return total;
}
}
}

#endif // PS4_PLATFORM
