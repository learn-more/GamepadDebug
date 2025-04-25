#include <cstdint>
#include <string>

using std::string;

namespace GD
{
    namespace XInput
    {
        struct DPad
        {
            uint8_t Up : 1;
            uint8_t Down : 1;
            uint8_t Left : 1;
            uint8_t Right : 1;

            void clear()
            {
                Up = 0;
                Down = 0;
                Left = 0;
                Right = 0;
            }
        };

        struct GamePadButtons
        {
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
            DPad DPad;

            GamePadButtons()
            {
                clear();
            }

            void clear()
            {
                DPad.clear();
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
            uint8_t plugInModules : 1;

            GamePadFeatures()
            {
                clear();
            }

            bool any() const
            {
                return voice || forceFeedback || wireless || noNavigation || plugInModules;
            }

            void clear()
            {
                voice = 0;
                forceFeedback = 0;
                wireless = 0;
                noNavigation = 0;
                plugInModules = 0;
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

    }
}
