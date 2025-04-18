#include <cstdint>
#include <string>

using std::string;

struct GamePadButtons
{
    uint16_t DPadUp : 1;
    uint16_t DPadDown : 1;
    uint16_t DPadLeft : 1;
    uint16_t DPadRight : 1;
    uint16_t Start : 1;
    uint16_t Back : 1;
    uint16_t LeftThumb : 1;
    uint16_t RightThumb : 1;
    uint16_t LeftShoulder : 1;
    uint16_t RightShoulder : 1;
    uint16_t A : 1;
    uint16_t B : 1;
    uint16_t X : 1;
    uint16_t Y : 1;
    uint16_t Guide : 1;

    GamePadButtons()
    {
        clear();
    }

    void clear()
    {
        DPadUp = 0;
        DPadDown = 0;
        DPadLeft = 0;
        DPadRight = 0;
        Start = 0;
        Back = 0;
        LeftThumb = 0;
        RightThumb = 0;
        LeftShoulder = 0;
        RightShoulder = 0;
        A = 0;
        B = 0;
        X = 0;
        Y = 0;
        Guide = 0;
    }
};

struct GamePadFeatures
{
    uint8_t voice : 1;
    uint8_t forceFeedback : 1;
    uint8_t wireless : 1;
    uint8_t noNavigation : 1;

    GamePadFeatures()
    {
        clear();
    }

    void clear()
    {
        voice = 0;
        forceFeedback = 0;
        wireless = 0;
        noNavigation = 0;
    }
};

struct GamePadState
{
    bool connected = false;
    uint32_t session = 0;
    string type;
    GamePadFeatures features;
    GamePadButtons buttons;

    void clear()
    {
        connected = false;
        session = 0;
        type.clear();
        features.clear();
        buttons.clear();
    }
};
