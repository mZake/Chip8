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

// Suffix pattern:
// - A: Address
// - B: Byte
// - N: Nibble
// - V: Register V
// - I: Register I
// - D: Delay Timer
// - S: Sound Timer
// - F: Placeholder
// - K: Keypad Key
// 
// These letters appear in the same order as the positional arguments of the instruction
enum opcode
{
    OPCODE_NONE,
    OPCODE_CLS,
    OPCODE_RET,
    OPCODE_JP_A,
    OPCODE_CALL_A,
    OPCODE_SE_VB,
    OPCODE_SNE_VB,
    OPCODE_SE_VV,
    OPCODE_LD_VB,
    OPCODE_ADD_VB,
    OPCODE_LD_VV,
    OPCODE_OR_VV,
    OPCODE_AND_VV,
    OPCODE_XOR_VV,
    OPCODE_ADD_VV,
    OPCODE_SUB_VV,
    OPCODE_SHR_V,
    OPCODE_SUBN_VV,
    OPCODE_SHL_V,
    OPCODE_SNE_VV,
    OPCODE_LD_IA,
    OPCODE_JP_VA,
    OPCODE_RND_VB,
    OPCODE_DRW_VVN,
    OPCODE_SKP_V,
    OPCODE_SKNP_V,
    OPCODE_LD_VD,
    OPCODE_LD_VK,
    OPCODE_LD_DV,
    OPCODE_LD_SV,
    OPCODE_ADD_IV,
    OPCODE_LD_FV,
    OPCODE_LD_BV,
    OPCODE_LD_IV,
    OPCODE_LD_VI,
};

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

struct instruction_data
{
    opcode Opcode;
    uint16_t NNN;
    uint8_t N;
    uint8_t X;
    uint8_t Y;
    uint8_t KK;
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

static uint16_t Chip8FetchInstruction(chip8_state& State)
{
    uint16_t Instruction = (State.Memory[State.RegisterPC] << 8) | State.Memory[State.RegisterPC + 1];
    State.RegisterPC += 2;
    return Instruction;
}

static instruction_data Chip8DecodeInstruction(uint16_t Instruction)
{
    instruction_data Data = {};
    
    switch (Instruction & 0xF000)
    {
        case 0x0000:
        {
            switch (Instruction & 0x000F)
            {
                case 0x0000: Data.Opcode = OPCODE_CLS; break;
                case 0x000E: Data.Opcode = OPCODE_RET; break;
            }
            break;
        }
        case 0x1000: Data.Opcode = OPCODE_JP_A; break;
        case 0x2000: Data.Opcode = OPCODE_CALL_A; break;
        case 0x3000: Data.Opcode = OPCODE_SE_VB; break;
        case 0x4000: Data.Opcode = OPCODE_SNE_VB; break;
        case 0x5000: Data.Opcode = OPCODE_SE_VV; break;
        case 0x6000: Data.Opcode = OPCODE_LD_VB; break;
        case 0x7000: Data.Opcode = OPCODE_ADD_VB; break;
        case 0x8000:
        {
            switch (Instruction & 0x000F)
            {
                case 0x0000: Data.Opcode = OPCODE_LD_VV; break;
                case 0x0001: Data.Opcode = OPCODE_OR_VV; break;
                case 0x0002: Data.Opcode = OPCODE_AND_VV; break;
                case 0x0003: Data.Opcode = OPCODE_XOR_VV; break;
                case 0x0004: Data.Opcode = OPCODE_ADD_VV; break;
                case 0x0005: Data.Opcode = OPCODE_SUB_VV; break;
                case 0x0006: Data.Opcode = OPCODE_SHR_V; break;
                case 0x0007: Data.Opcode = OPCODE_SUBN_VV; break;
                case 0x000E: Data.Opcode = OPCODE_SHL_V; break;
            }
            break;
        }
        case 0x9000: Data.Opcode = OPCODE_SNE_VV; break;
        case 0xA000: Data.Opcode = OPCODE_LD_IA; break;
        case 0xB000: Data.Opcode = OPCODE_JP_VA; break;
        case 0xC000: Data.Opcode = OPCODE_RND_VB; break;
        case 0xD000: Data.Opcode = OPCODE_DRW_VVN; break;
        case 0xE000:
        {
            switch (Instruction & 0x00FF)
            {
                case 0x009E: Data.Opcode = OPCODE_SKP_V; break;
                case 0x00A1: Data.Opcode = OPCODE_SKNP_V; break;
            }
            break;
        }
        case 0xF000:
        {
            switch (Instruction & 0x00FF)
            {
                case 0x0007: Data.Opcode = OPCODE_LD_VD; break;
                case 0x000A: Data.Opcode = OPCODE_LD_VK; break;
                case 0x0015: Data.Opcode = OPCODE_LD_DV; break;
                case 0x0018: Data.Opcode = OPCODE_LD_SV; break;
                case 0x001E: Data.Opcode = OPCODE_ADD_IV; break;
                case 0x0029: Data.Opcode = OPCODE_LD_FV; break;
                case 0x0033: Data.Opcode = OPCODE_LD_BV; break;
                case 0x0055: Data.Opcode = OPCODE_LD_IV; break;
                case 0x0065: Data.Opcode = OPCODE_LD_VI; break;
            }
            break;
        }
        default: Data.Opcode = OPCODE_NONE; break;
    }
    
    Data.NNN = (Instruction & 0x0FFF);
    Data.N = (Instruction & 0x000F);
    Data.X = (Instruction & 0x0F00) >> 8;
    Data.Y = (Instruction & 0x00F0) >> 4;
    Data.KK = (Instruction & 0x00FF);
    
    return Data;
}

static void Chip8ExecuteInstruction(chip8_state& State, instruction_data Data)
{
    switch (Data.Opcode)
    {
        case OPCODE_NONE:
            break;
        // 00E0 - CLS
        // Clear the display.
        case OPCODE_CLS:
        {
            memset(State.Screen.data(), 0, State.Screen.size());
            break;
        }
        // 00EE - RET
        // Return from a subroutine.
        case OPCODE_RET:
        {
            State.RegisterPC = State.Stack[--State.RegisterSP];
            break;
        }
        // 1nnn - JP addr
        // Jump to location nnn.
        case OPCODE_JP_A:
        {
            State.RegisterPC = Data.NNN;
            break;
        }
        // 2nnn - CALL addr
        // Call subroutine at nnn.
        case OPCODE_CALL_A:
        {
            State.Stack[State.RegisterSP++] = State.RegisterPC;
            State.RegisterPC = Data.NNN;
            break;
        }
        // 3xkk - SE Vx, byte
        // Skip next instruction if Vx = kk.
        case OPCODE_SE_VB:
        {
            if (State.RegisterV[Data.X] == Data.KK)
                State.RegisterPC += 2;
            break;
        }
        // 4xkk - SNE Vx, byte
        // Skip next instruction if Vx != kk.
        case OPCODE_SNE_VB:
        {
            if (State.RegisterV[Data.X] != Data.KK)
                State.RegisterPC += 2;
            break;
        }
        // 5xy0 - SE Vx, Vy
        // Skip next instruction if Vx = Vy.
        case OPCODE_SE_VV:
        {
            if (State.RegisterV[Data.X] == State.RegisterV[Data.Y])
                State.RegisterPC += 2;
            break;
        }
        // 6xkk - LD Vx, byte
        // Set Vx = kk.
        case OPCODE_LD_VB:
        {
            State.RegisterV[Data.X] = Data.KK;
            break;
        }
        // 7xkk - ADD Vx, byte
        // Set Vx = Vx + kk.
        case OPCODE_ADD_VB:
        {
            State.RegisterV[Data.X] += Data.KK;
            break;
        }
        // 8xy0 - LD Vx, Vy
        // Set Vx = Vy.
        case OPCODE_LD_VV:
        {
            State.RegisterV[Data.X] = State.RegisterV[Data.Y];
            break;
        }
        // 8xy1 - OR Vx, Vy
        // Set Vx = Vx OR Vy.
        case OPCODE_OR_VV:
        {
            State.RegisterV[Data.X] |= State.RegisterV[Data.Y];
            break;
        }
        // 8xy2 - AND Vx, Vy
        // Set Vx = Vx AND Vy.
        case OPCODE_AND_VV:
        {
            State.RegisterV[Data.X] &= State.RegisterV[Data.Y];
            break;
        }
        // 8xy3 - XOR Vx, Vy
        // Set Vx = Vx XOR Vy.
        case OPCODE_XOR_VV:
        {
            State.RegisterV[Data.X] ^= State.RegisterV[Data.Y];
            break;
        }
        // 8xy4 - ADD Vx, Vy
        // Set Vx = Vx + Vy, set VF = carry.
        case OPCODE_ADD_VV:
        {
            uint16_t Result = State.RegisterV[Data.X] + State.RegisterV[Data.Y];
            State.RegisterV[Data.X] = Result & 0x00FF;
            State.RegisterV[0xF] = (Result > 0xFF);
            break;
        }
        // 8xy5 - SUB Vx, Vy
        // Set Vx = Vx - Vy, set VF = NOT borrow.
        case OPCODE_SUB_VV:
        {
            State.RegisterV[0xF] = (State.RegisterV[Data.X] >= State.RegisterV[Data.Y]);
            State.RegisterV[Data.X] -= State.RegisterV[Data.Y];
            break;
        }
        // 8xy6 - SHR Vx {, Vy}
        // Set Vx = Vx SHR 1.
        case OPCODE_SHR_V:
        {
            State.RegisterV[0xF] = State.RegisterV[Data.X] & 0x01;
            State.RegisterV[Data.X] >>= 1;
            break;
        }
        // 8xy7 - SUBN Vx, Vy
        // Set Vx = Vy - Vx, set VF = NOT borrow.
        case OPCODE_SUBN_VV:
        {
            State.RegisterV[0xF] = (State.RegisterV[Data.Y] >= State.RegisterV[Data.X]);
            State.RegisterV[Data.X] = State.RegisterV[Data.Y] - State.RegisterV[Data.X];
            break;
        }
        // 8xyE - SHL Vx {, Vy}
        // Set Vx = Vx SHL 1.
        case OPCODE_SHL_V:
        {
            State.RegisterV[0xF] = (State.RegisterV[Data.X] & 0x80) >> 7;
            State.RegisterV[Data.X] <<= 1;
            break;
        }
        break;
        // 9xy0 - SNE Vx, Vy
        // Skip next instruction if Vx != Vy.
        case OPCODE_SNE_VV:
        {
            if (State.RegisterV[Data.X] != State.RegisterV[Data.Y])
                State.RegisterPC += 2;
            break;
        }
        // Annn - LD I, addr
        // Set I = nnn.
        case OPCODE_LD_IA:
        {
            State.RegisterI = Data.NNN;
            break;
        }
        // Bnnn - JP V0, addr
        // Jump to location nnn + V0.
        case OPCODE_JP_VA:
        {
            State.RegisterPC = Data.NNN + State.RegisterV[0x0];
            break;
        }
        // Cxkk - RND Vx, byte
        // Set Vx = random byte AND kk.
        case OPCODE_RND_VB:
        {
            State.RegisterV[Data.X] = GetRandomByte() & Data.KK;
            break;
        }
        // Dxyn - DRW Vx, Vy, nibble
        // Display n-byte sprite starting at Memory location I at (Vx, Vy), set VF = collision.
        case OPCODE_DRW_VVN:
        {
            uint8_t StartX = State.RegisterV[Data.X] % SCREEN_WIDTH;
            uint8_t StartY = State.RegisterV[Data.Y] % SCREEN_HEIGHT;
            
            State.RegisterV[0xF] = 0;
            State.ShouldDraw = true;
            
            for (int Row = 0; Row < Data.N; ++Row)
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
        // Ex9E - SKP Vx
        // Skip next instruction if key with the value of Vx is pressed.
        case OPCODE_SKP_V:
        {
            uint8_t Key = State.RegisterV[Data.X];
            if (State.Keypad[Key])
                State.RegisterPC += 2;
            break;
        }
        // ExA1 - SKNP Vx
        // Skip next instruction if key with the value of Vx is not pressed.
        case OPCODE_SKNP_V:
        {
            uint8_t Key = State.RegisterV[Data.X];
            if (!State.Keypad[Key])
                State.RegisterPC += 2;
            break;
        }
        // Fx07 - LD Vx, DT
        // Set Vx = delay timer value.
        case OPCODE_LD_VD:
        {
            State.RegisterV[Data.X] = State.DelayTimer;
            break;
        }
        // Fx0A - LD Vx, K
        // Wait for a key press, store the value of the key in Vx.
        case OPCODE_LD_VK:
        {
            bool KeyPressed = false;
            for (int Key = 0; Key < 16; ++Key)
            {
                if (State.Keypad[Key] != 0)
                {
                    State.RegisterV[Data.X] = Key;
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
        case OPCODE_LD_DV:
        {
            State.DelayTimer = State.RegisterV[Data.X];
            break;
        }
        // Fx18 - LD ST, Vx
        // Set sound timer = Vx.
        case OPCODE_LD_SV:
        {
            State.SoundTimer = State.RegisterV[Data.X];
            break;
        }
        // Fx1E - ADD I, Vx
        // Set I = I + Vx.
        case OPCODE_ADD_IV:
        {
            State.RegisterI += State.RegisterV[Data.X];
            break;
        }
        // Fx29 - LD F, Vx
        // Set I = location of sprite for digit Vx.
        case OPCODE_LD_FV:
        {
            State.RegisterI = FONTSET_ADDRESS + State.RegisterV[Data.X] * FONT_SIZE;
            break;
        }
        // Fx33 - LD B, Vx
        // Store BCD representation of Vx in Memory locations I, I+1, and I+2.
        case OPCODE_LD_BV:
        {
            State.Memory[State.RegisterI] = State.RegisterV[Data.X] / 100;
            State.Memory[State.RegisterI + 1] = (State.RegisterV[Data.X] / 10) % 10;
            State.Memory[State.RegisterI + 2] = State.RegisterV[Data.X] % 10;
            break;
        }
        // Fx55 - LD [I], Vx
        // Store registers V0 through Vx in Memory starting at location I.
        case OPCODE_LD_IV:
        {
            for (int Index = 0; Index <= Data.X; ++Index)
                State.Memory[State.RegisterI + Index] = State.RegisterV[Index];
            break;
        }
        // Fx65 - LD Vx, [I]
        // Read registers V0 through Vx from Memory starting at location I.
        case OPCODE_LD_VI:
        {
            for (int Index = 0; Index <= Data.X; ++Index)
                State.RegisterV[Index] = State.Memory[State.RegisterI + Index];
            break;
        }
    }
}

static void Chip8EmulateInstructionCycle(chip8_state& State)
{
    uint16_t Instruction = Chip8FetchInstruction(State);
    instruction_data Data = Chip8DecodeInstruction(Instruction);
    Chip8ExecuteInstruction(State, Data);
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
