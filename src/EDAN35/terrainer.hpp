#pragma once


class InputHandler;
class Window;


namespace edan35
{
    //! \brief Wrapper class for Assignment 2
    class Terrainer {
    public:
        //! \brief Default constructor.
        //!
        //! It will initialise various modules of bonobo and retrieve a
        //! window to draw to.
        Terrainer();

        //! \brief Default destructor.
        //!
        //! It will release the bonobo modules initialised by the
        //! constructor, as well as the window.
        ~Terrainer();

        //! \brief Contains the logic of the assignment, along with the
        //! render loop.
        void run();

    private:
        InputHandler *inputHandler;
        Window       *window;
    };
}