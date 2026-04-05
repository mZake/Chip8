#include <array>
#include <filesystem>
#include <fstream>
#include <random>
#include <string_view>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <SDL3/SDL.h>

constexpr int SCREEN_WIDTH = 64;
constexpr int SCREEN_HEIGHT = 32;

constexpr int PROGRAM_ADDRESS = 0x200;
constexpr int FONTSET_ADDRESS = 0x0;

constexpr int FONT_SIZE = 0x5;
constexpr int MAX_PROGRAM_SIZE = 0xE00;

constexpr double FRAME_TIME = 1000.0 / 60.0;

struct platform_state
{
    SDL_Window* Window;
    SDL_Renderer* Renderer;
    const bool* Keyboard;
    bool ShouldQuit;
};

struct chip8_state
{
    std::array<uint8_t, 4096> Memory;
    std::array<uint8_t, 2048> Screen;
    std::array<uint16_t, 16> Stack;
    std::array<uint8_t, 16> Keypad;
    std::array<uint8_t, 16> RegisterV;
    uint16_t RegisterPC;
    uint16_t RegisterSP;
    uint16_t RegisterI;
    uint8_t DelayTimer;
    uint8_t SoundTimer;
    bool ShouldDraw;
    bool ShouldPlaySound;
};

static std::array<uint8_t, 80> s_Fontset = { 
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

static uint8_t GetRandomByte()
{
    static std::random_device Device;
    static std::mt19937 Generator(Device());
    static std::uniform_int_distribution<uint8_t> Distributor(0, 255);
    uint8_t Result = Distributor(Generator);
    return Result;
}

static bool InitPlatform(platform_state& State)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return false;
    }
    
    State.Window = SDL_CreateWindow("CHIP-8", 1024, 512, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    if (!State.Window)
    {
        return false;
    }
    
    State.Renderer = SDL_CreateRenderer(State.Window, nullptr);
    if (!State.Renderer)
    {
        return false;
    }
    
    State.Keyboard = SDL_GetKeyboardState(nullptr);
    State.ShouldQuit = false;
    
    SDL_SetRenderLogicalPresentation(State.Renderer, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_LOGICAL_PRESENTATION_STRETCH);
    SDL_SetRenderVSync(State.Renderer, 1);
    
    SDL_ShowWindow(State.Window);
    
    return true;
}

static void ClosePlatform(platform_state& State)
{
    if (State.Renderer)
    {
        SDL_DestroyRenderer(State.Renderer);
    }
    
    if (State.Window)
    {
        SDL_DestroyWindow(State.Window);
    }
    
    SDL_Quit();
}

static void PlatformProcessEvents(platform_state& State)
{
    SDL_Event Event = {};
    while (SDL_PollEvent(&Event))
    {
        if (Event.type == SDL_EVENT_QUIT)
        {
            State.ShouldQuit = true;
        }
    }
}

static void PlatformUpdateKeypad(platform_state& State, std::array<uint8_t, 16>& Keypad)
{
    Keypad[0x0] = State.Keyboard[SDL_SCANCODE_X];
    Keypad[0x1] = State.Keyboard[SDL_SCANCODE_1];
    Keypad[0x2] = State.Keyboard[SDL_SCANCODE_2];
    Keypad[0x3] = State.Keyboard[SDL_SCANCODE_3];
    Keypad[0x4] = State.Keyboard[SDL_SCANCODE_Q];
    Keypad[0x5] = State.Keyboard[SDL_SCANCODE_W];
    Keypad[0x6] = State.Keyboard[SDL_SCANCODE_E];
    Keypad[0x7] = State.Keyboard[SDL_SCANCODE_A];
    Keypad[0x8] = State.Keyboard[SDL_SCANCODE_S];
    Keypad[0x9] = State.Keyboard[SDL_SCANCODE_D];
    Keypad[0xA] = State.Keyboard[SDL_SCANCODE_Z];
    Keypad[0xB] = State.Keyboard[SDL_SCANCODE_C];
    Keypad[0xC] = State.Keyboard[SDL_SCANCODE_4];
    Keypad[0xD] = State.Keyboard[SDL_SCANCODE_R];
    Keypad[0xE] = State.Keyboard[SDL_SCANCODE_F];
    Keypad[0xF] = State.Keyboard[SDL_SCANCODE_V];
}

static void PlatformRenderScreen(platform_state& State, const std::array<uint8_t, 2048>& Screen)
{
    SDL_SetRenderDrawColor(State.Renderer, 0, 0, 0, 255);
    SDL_RenderClear(State.Renderer);
    SDL_SetRenderDrawColor(State.Renderer, 255, 255, 255, 255);
    
    for (int Y = 0; Y < SCREEN_HEIGHT; ++Y)
    {
        for (int X = 0; X < SCREEN_WIDTH; ++X)
        {
            uint8_t Pixel = Screen[X + Y * SCREEN_WIDTH];
            if (Pixel != 0)
            {
                SDL_RenderPoint(State.Renderer, X, Y);
            }
        }
    }
    
    SDL_RenderPresent(State.Renderer);
}

static bool Chip8LoadProgram(chip8_state& State, std::string_view Filepath)
{
    std::ifstream Stream(Filepath.data(), std::ios::binary);
    if (!Stream.is_open())
    {
        return false;
    }
    
    size_t FileSize = std::filesystem::file_size(Filepath);
    if (FileSize > MAX_PROGRAM_SIZE)
    {
        return false;
    }
    
    std::array<char, MAX_PROGRAM_SIZE> Buffer;
    Stream.read(Buffer.data(), FileSize);
    
    memset(&State, 0, sizeof(State));
    memcpy(State.Memory.data() + FONTSET_ADDRESS, s_Fontset.data(), sizeof(s_Fontset));
    memcpy(State.Memory.data() + PROGRAM_ADDRESS, Buffer.data(), FileSize);
    
    State.RegisterPC = PROGRAM_ADDRESS;
    
    return true;
}

static void Chip8EmulateInstructionCycle(chip8_state& State)
{
    uint16_t Instruction = (State.Memory[State.RegisterPC] << 8) | State.Memory[State.RegisterPC + 1];
    State.RegisterPC += 2;
    switch (Instruction & 0xF000)
    {
        case 0x0000:
        {
            switch (Instruction & 0x000F)
            {
                // 00E0 - CLS
                // Clear the display.
                case 0x0000:
                {
                    memset(State.Screen.data(), 0, State.Screen.size());
                    break;
                }
                // 00EE - RET
                // Return from a subroutine.
                case 0x000E:
                {
                    State.RegisterPC = State.Stack[--State.RegisterSP];
                    break;
                }
            }
            break;
        }
        // 1nnn - JP addr
        // Jump to location nnn.
        case 0x1000:
        {
            uint16_t Address = Instruction & 0x0FFF;
            State.RegisterPC = Address;
            break;
        }
        // 2nnn - CALL addr
        // Call subroutine at nnn.
        case 0x2000:
        {
            uint16_t Address = Instruction & 0x0FFF;
            State.Stack[State.RegisterSP++] = State.RegisterPC;
            State.RegisterPC = Address;
            break;
        }
        // 3xkk - SE Vx, byte
        // Skip next instruction if Vx = kk.
        case 0x3000:
        {
            uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
            uint8_t Byte = (Instruction & 0x00FF);
            if (State.RegisterV[RegisterV] == Byte)
                State.RegisterPC += 2;
            break;
        }
        // 4xkk - SNE Vx, byte
        // Skip next instruction if Vx != kk.
        case 0x4000:
        {
            uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
            uint8_t Byte = (Instruction & 0x00FF);
            if (State.RegisterV[RegisterV] != Byte)
                State.RegisterPC += 2;
            break;
        }
        // 5xy0 - SE Vx, Vy
        // Skip next instruction if Vx = Vy.
        case 0x5000:
        {
            uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
            uint8_t Y = (Instruction & 0x00F0) >> 4;
            if (State.RegisterV[RegisterV] == State.RegisterV[Y])
                State.RegisterPC += 2;
            break;
        }
        // 6xkk - LD Vx, byte
        // Set Vx = kk.
        case 0x6000:
        {
            uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
            uint8_t Byte = Instruction & 0x00FF;
            State.RegisterV[RegisterV] = Byte;
            break;
        }
        // 7xkk - ADD Vx, byte
        // Set Vx = Vx + kk.
        case 0x7000:
        {
            uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
            uint8_t Byte = Instruction & 0x00FF;
            State.RegisterV[RegisterV] += Byte;
            break;
        }
        case 0x8000:
        {
            switch (Instruction & 0x000F)
            {
                // 8xy0 - LD Vx, Vy
                // Set Vx = Vy.
                case 0x0000:
                {
                    uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
                    uint8_t Y = (Instruction & 0x00F0) >> 4;
                    State.RegisterV[RegisterV] = State.RegisterV[Y];
                    break;
                }
                // 8xy1 - OR Vx, Vy
                // Set Vx = Vx OR Vy.
                case 0x0001:
                {
                    uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
                    uint8_t Y = (Instruction & 0x00F0) >> 4;
                    State.RegisterV[RegisterV] |= State.RegisterV[Y];
                    break;
                }
                // 8xy2 - AND Vx, Vy
                // Set Vx = Vx AND Vy.
                case 0x0002:
                {
                    uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
                    uint8_t Y = (Instruction & 0x00F0) >> 4;
                    State.RegisterV[RegisterV] &= State.RegisterV[Y];
                    break;
                }
                // 8xy3 - XOR Vx, Vy
                // Set Vx = Vx XOR Vy.
                case 0x0003:
                {
                    uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
                    uint8_t Y = (Instruction & 0x00F0) >> 4;
                    State.RegisterV[RegisterV] ^= State.RegisterV[Y];
                    break;
                }
                // 8xy4 - ADD Vx, Vy
                // Set Vx = Vx + Vy, set VF = carry.
                case 0x0004:
                {
                    uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
                    uint8_t Y = (Instruction & 0x00F0) >> 4;
                    uint16_t Result = State.RegisterV[RegisterV] + State.RegisterV[Y];
                    State.RegisterV[RegisterV] = Result & 0x00FF;
                    State.RegisterV[0xF] = (Result > 0xFF);
                    break;
                }
                // 8xy5 - SUB Vx, Vy
                // Set Vx = Vx - Vy, set VF = NOT borrow.
                case 0x0005:
                {
                    uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
                    uint8_t Y = (Instruction & 0x00F0) >> 4;
                    State.RegisterV[0xF] = (State.RegisterV[RegisterV] >= State.RegisterV[Y]);
                    State.RegisterV[RegisterV] -= State.RegisterV[Y];
                    break;
                }
                // 8xy6 - SHR Vx {, Vy}
                // Set Vx = Vx SHR 1.
                case 0x0006:
                {
                    uint8_t RegisterV = (Instruction & 0x0F00) >> 8;
                    State.RegisterV[0xF] = State.RegisterV[RegisterV] & 0x01;
                    State.RegisterV[RegisterV] >>= 1;
                    break;
                }
                // 8xy7 - SUBN Vx, Vy
                // Set Vx = Vy - Vx, set VF = NOT borrow.
                case 0x0007:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    uint8_t Y = (Instruction & 0x00F0) >> 4;
                    State.RegisterV[0xF] = (State.RegisterV[Y] >= State.RegisterV[X]);
                    State.RegisterV[X] = State.RegisterV[Y] - State.RegisterV[X];
                    break;
                }
                // 8xyE - SHL Vx {, Vy}
                // Set Vx = Vx SHL 1.
                case 0x000E:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    State.RegisterV[0xF] = (State.RegisterV[X] & 0x80) >> 7;
                    State.RegisterV[X] <<= 1;
                    break;
                }
            }
            break;
        }
        // 9xy0 - SNE Vx, Vy
        // Skip next instruction if Vx != Vy.
        case 0x9000:
        {
            uint8_t X = (Instruction & 0x0F00) >> 8;
            uint8_t Y = (Instruction & 0x00F0) >> 4;
            if (State.RegisterV[X] != State.RegisterV[Y])
                State.RegisterPC += 2;
            break;
        }
        // Annn - LD I, addr
        // Set I = nnn.
        case 0xA000:
        {
            uint16_t Address = Instruction & 0x0FFF;
            State.RegisterI = Address;
            break;
        }
        // Bnnn - JP V0, addr
        // Jump to location nnn + V0.
        case 0xB000:
        {
            uint16_t Address = Instruction & 0x0FFF;
            State.RegisterPC = Address + State.RegisterV[0x0];
            break;
        }
        // Cxkk - RND Vx, byte
        // Set Vx = random byte AND kk.
        case 0xC000:
        {
            uint8_t X = (Instruction & 0x0F00) >> 8;
            uint8_t Byte = Instruction & 0x00FF;
            State.RegisterV[X] = GetRandomByte() & Byte;
            break;
        }
        // Dxyn - DRW Vx, Vy, nibble
        // Display n-byte sprite starting at Memory location I at (Vx, Vy), set VF = collision.
        case 0xD000:
        {
            uint8_t X = (Instruction & 0x0F00) >> 8;
            uint8_t Y = (Instruction & 0x00F0) >> 4;
            uint8_t Height = Instruction & 0x000F;
            uint8_t StartX = State.RegisterV[X] % SCREEN_WIDTH;
            uint8_t StartY = State.RegisterV[Y] % SCREEN_HEIGHT;
            
            State.RegisterV[0xF] = 0;
            State.ShouldDraw = true;
            
            for (int Row = 0; Row < Height; ++Row)
            {
                uint8_t Sprite = State.Memory[State.RegisterI + Row];
                for (int Column = 0; Column < 8; ++Column)
                {
                    uint8_t PositionX = StartX + Column;
                    uint8_t PositionY = StartY + Row;
                    if (PositionX >= SCREEN_WIDTH || PositionY >= SCREEN_HEIGHT)
                        continue;
                    
                    uint8_t Pixel = Sprite & (0x80 >> Column);
                    if (Pixel != 0)
                    {
                        uint16_t Index = PositionX + PositionY * SCREEN_WIDTH;
                        if (State.Screen[Index] == 1)
                            State.RegisterV[0xF] = 1;
                        State.Screen[Index] ^= 1;
                    }
                }
            }
            break;
        }
        case 0xE000:
        {
            switch (Instruction & 0x00FF)
            {
                // Ex9E - SKP Vx
                // Skip next instruction if key with the value of Vx is pressed.
                case 0x009E:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    uint8_t Key = State.RegisterV[X];
                    if (State.Keypad[Key])
                        State.RegisterPC += 2;
                    break;
                }
                // ExA1 - SKNP Vx
                // Skip next instruction if key with the value of Vx is not pressed.
                case 0x00A1:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    uint8_t Key = State.RegisterV[X];
                    if (!State.Keypad[Key])
                        State.RegisterPC += 2;
                    break;
                }
            }
            break;
        }
        case 0xF000:
        {
            switch (Instruction & 0x00FF)
            {
                // Fx07 - LD Vx, DT
                // Set Vx = delay timer value.
                case 0x0007:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    State.RegisterV[X] = State.DelayTimer;
                    break;
                }
                // Fx0A - LD Vx, K
                // Wait for a key press, store the value of the key in Vx.
                case 0x000A:
                {
                    bool KeyPressed = false;
                    for (int Key = 0; Key < 16; ++Key)
                    {
                        if (State.Keypad[Key] != 0)
                        {
                            uint8_t X = (Instruction & 0x0F00) >> 8;
                            State.RegisterV[X] = Key;
                            KeyPressed = true;
                        }
                    }
                    if (!KeyPressed)
                    {
                        State.RegisterPC -= 2;
                        return;
                    }
                    break;
                }
                // Fx15 - LD DT, Vx
                // Set delay timer = Vx.
                case 0x0015:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    State.DelayTimer = State.RegisterV[X];
                    break;
                }
                // Fx18 - LD ST, Vx
                // Set sound timer = Vx.
                case 0x0018:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    State.SoundTimer = State.RegisterV[X];
                    break;
                }
                // Fx1E - ADD I, Vx
                // Set I = I + Vx.
                case 0x001E:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    State.RegisterI += State.RegisterV[X];
                    break;
                }
                // Fx29 - LD F, Vx
                // Set I = location of sprite for digit Vx.
                case 0x0029:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    State.RegisterI = FONTSET_ADDRESS + State.RegisterV[X] * FONT_SIZE;
                    break;
                }
                // Fx33 - LD B, Vx
                // Store BCD representation of Vx in Memory locations I, I+1, and I+2.
                case 0x0033:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    State.Memory[State.RegisterI] = State.RegisterV[X] / 100;
                    State.Memory[State.RegisterI + 1] = (State.RegisterV[X] / 10) % 10;
                    State.Memory[State.RegisterI + 2] = State.RegisterV[X] % 10;
                    break;
                }
                // Fx55 - LD [I], Vx
                // Store registers V0 through Vx in Memory starting at location I.
                case 0x0055:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    for (int Index = 0; Index <= X; ++Index)
                        State.Memory[State.RegisterI + Index] = State.RegisterV[Index];
                    break;
                }
                // Fx65 - LD Vx, [I]
                // Read registers V0 through Vx from Memory starting at location I.
                case 0x0065:
                {
                    uint8_t X = (Instruction & 0x0F00) >> 8;
                    for (int Index = 0; Index <= X; ++Index)
                        State.RegisterV[Index] = State.Memory[State.RegisterI + Index];
                    break;
                }
            }
            break;
        }
    }
}

static void Chip8UpdateTimers(chip8_state& State)
{
    if (State.DelayTimer > 0)
    {
        --State.DelayTimer;
    }
    
    if (State.SoundTimer > 0)
    {
        --State.SoundTimer;
    }
}

int main(int Argc, char** Argv)
{
    if (Argc != 2)
    {
        fprintf(stderr, "Usage: chip8 <program>\n");
        return 1;
    }
    
    platform_state PlatformState = {};
    if (!InitPlatform(PlatformState))
    {
        fprintf(stderr, "Error: failed to initialize platform\n");
        return 1;
    }
    
    chip8_state Chip8State = {};
    if (!Chip8LoadProgram(Chip8State, Argv[1]))
    {
        fprintf(stderr, "Error: failed to load program %s\n", Argv[1]);
        return 1;
    }
    
    double Accumulator = 0.0;
    uint64_t LastTicks = SDL_GetTicks();
    
    while (!PlatformState.ShouldQuit)
    {
        uint64_t CurrentTicks = SDL_GetTicks();
        Accumulator += (CurrentTicks - LastTicks);
        LastTicks = CurrentTicks;
        
        while (Accumulator >= FRAME_TIME)
        {
            PlatformProcessEvents(PlatformState);
            PlatformUpdateKeypad(PlatformState, Chip8State.Keypad);
            
            for (int I = 0; I < 10; ++I)
            {
                Chip8EmulateInstructionCycle(Chip8State);
            }
            
            Chip8UpdateTimers(Chip8State);
            
            if (Chip8State.ShouldDraw)
            {
                Chip8State.ShouldDraw = false;
                PlatformRenderScreen(PlatformState, Chip8State.Screen);
            }
            
            Accumulator -= FRAME_TIME;
        }
    }
    
    ClosePlatform(PlatformState);
}
