#include "refl.h"

#include <GL/freeglut.h>
#include <sstream>

struct Node
{
    int                     x,y;
    const Function *        function; // If non-null, this is a function node
    std::vector<size_t>     inputs;
    VarType                 type;
    std::shared_ptr<void>   value;
};
std::vector<Node> nodes;

void OnIdle() 
{ 
    glutPostRedisplay(); 
}

void RenderText(int x, int y, const std::string & text)
{
    glRasterPos2i(x,y);
    for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, ch);
}

void OnDisplay()
{  
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glOrtho(0, 1280, 720, 0, -1, 1);

    for(const auto & n : nodes)
    {
        std::ostringstream ss;
        if(n.function) ss << *n.function;
        else ss << n.type;
        ss << ": ";
        if(n.type.type->index == typeid(int)) ss << *(const int *)n.value.get();
        RenderText(n.x, n.y, ss.str());

        for(size_t i=0; i<n.inputs.size(); ++i)
        {
            ss.str("");
            ss << n.function->GetParamTypes()[i];
            RenderText(n.x, n.y + (i+1)*16, ss.str());
            glBegin(GL_LINES);
            glVertex2i(n.x, n.y + (i+1)*16);
            glVertex2i(nodes[n.inputs[i]].x, nodes[n.inputs[i]].y);
            glEnd();
        }
    }
   
    glPopMatrix();   
    glPopAttrib();

    glutSwapBuffers();
}

int add(int a, int b) { return a+b; }
int mul(int a, int b) { return a*b; }

int main(int argc, char * argv[])
{
    TypeLibrary types;
    types.BindFunction(&add,"add");
    types.BindFunction(&mul,"mul");

    nodes.resize(5);

    nodes[0].x = 100;
    nodes[0].y = 100;
    nodes[0].function = nullptr;
    nodes[0].type = types.DeduceVarType<int>();
    nodes[0].value = std::make_shared<int>(2);

    nodes[1].x = 100;
    nodes[1].y = 300;
    nodes[1].function = nullptr;
    nodes[1].type = types.DeduceVarType<int>();
    nodes[1].value = std::make_shared<int>(3);

    nodes[2].x = 500;
    nodes[2].y = 200;
    nodes[2].function = types.GetFunction("add");
    nodes[2].inputs = {0,1};
    nodes[2].type = {};
    nodes[2].value = nullptr;

    nodes[3].x = 500;
    nodes[3].y = 400;
    nodes[3].function = nullptr;
    nodes[3].type = types.DeduceVarType<int>();
    nodes[3].value = std::make_shared<int>(8);

    nodes[4].x = 900;
    nodes[4].y = 300;
    nodes[4].function = types.GetFunction("mul");
    nodes[4].inputs = {2,3};
    nodes[4].type = {};
    nodes[4].value = nullptr;

    // Evaluate call graph
    void * args[8];
    for(auto & n : nodes)
    {
        if(n.function)
        {
            for(size_t i=0; i<n.inputs.size(); ++i)
            {
                args[i] = nodes[n.inputs[i]].value.get();
            }
            n.value = n.function->Invoke(args);
            n.type = n.function->GetReturnType();
        }    
    }

    glutInit(&argc, argv);
    glutInitWindowSize(1280, 720);
    glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);
    glutCreateWindow("libmirror call graph visualizer");

    glutIdleFunc(OnIdle);
    glutDisplayFunc(OnDisplay);
    //glutReshapeFunc(OnReshape);
    //glutMotionFunc(OnMotion);
    //glutMouseFunc(OnMouse);
    //glutKeyboardFunc(OnKeyboard);
    glutMainLoop();

    return 0;
}