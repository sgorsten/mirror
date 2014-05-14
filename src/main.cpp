#include "refl.h"

#include <GL/freeglut.h>
#include <algorithm>
#include <sstream>

struct Rect { int x0,y0,x1,y1; };

class ToString
{
    std::ostringstream ss;
public:
    operator std::string() const { return ss.str(); }
    template<class T> ToString & operator << (const T & val) { ss << val; return *this; }
};

size_t GetStringWidth(const std::string & text)
{
    size_t w = 0;
    for(auto ch : text) w += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, ch);
    return w;
}

void WriteValue(std::ostream & out, const Type & type, void * value)
{
    switch(type.kind)
    {
    case Type::Fundamental:
        if(type.index == typeid(int)) out << *(int *)value;
        else out << "???";
        break;
    default:
        out << "???";
        break;
    }
}

std::string ToStr(const Type & type, void * value)
{
    std::ostringstream ss;
    WriteValue(ss, type, value);
    return ss.str();
}

struct Node
{
    int                     x,y;
    const Function *        function; // If non-null, this is a function node
    std::vector<int>        inputs;
    VarType                 type;
    std::shared_ptr<void>   value;

                            Node() : x(), y(), function() {}
                            Node(int x, int y, const Function * function) : x(x), y(y), function(function), inputs(GetInputCount(),-1), type(function->GetReturnType()) {}
    template<class T>       Node(int x, int y, TypeLibrary & types, T && value) : x(x), y(y), function(), type(types.DeduceVarType<T>()), value(std::make_shared<T>(std::move(value))) {}

    size_t                  GetInputCount() const { return function ? function->GetParamTypes().size() : 0; }
    size_t                  GetOutputCount() const { return 1; }

    size_t                  GetPinSize() const { return 22; }
    size_t                  GetPinPadding() const { return 2; }
    size_t                  GetColumnPadding() const { return 10; }

    size_t                  GetPinSpacing() const { return GetPinSize() + GetPinPadding(); }
    size_t                  GetPinColumnSize(size_t pins) const { return pins>0 ? (GetPinSize() + GetPinSpacing()*(pins-1)) : 0; }
    size_t                  GetInputColumnSize() const { return GetPinColumnSize(GetInputCount()); }
    size_t                  GetContentsColumnSize() const { return GetPinColumnSize(function ? 2 : 1); }
    size_t                  GetOutputColumnSize() const { return GetPinColumnSize(GetOutputCount()); }
    size_t                  GetSizeY() const { return std::max<size_t>({GetInputColumnSize(), GetContentsColumnSize(), GetOutputColumnSize()}); }

    size_t                  GetInputColumnLabelWidth() const { if(!function) return 0; size_t w=0; for(auto & p : function->GetParamTypes()) w = std::max(w, GetStringWidth(ToString() << p)); return w; }
    size_t                  GetContentsColumnWidth() const { return std::max(function ? GetStringWidth(function->GetName()) : 0, GetStringWidth(ToStr(*type.type, value.get()))); }
    size_t                  GetOutputColumnLabelWidth() const { return GetStringWidth(ToString() << type); }
    size_t                  GetSizeX() const { return GetPinSpacing() * 2 + GetInputColumnLabelWidth() + GetContentsColumnWidth() + GetOutputColumnLabelWidth() + GetColumnPadding() * 2; }

    Rect                    GetNodeRect() const { return {x,y,x+GetSizeX(),y+GetSizeY()}; }
    Rect                    GetPinRect(int x, int y) const { return {x,y,x+GetPinSize(),y+GetPinSize()}; }
    Rect                    GetInputPinRect(size_t i) const { return GetPinRect(x, y + (GetSizeY()-GetInputColumnSize())/2 + i*GetPinSpacing()); }
    Rect                    GetOutputPinRect(size_t i) const { return GetPinRect(x+GetSizeX()-GetPinSize(), y + (GetSizeY()-GetOutputColumnSize())/2 + i*GetPinSpacing()); }
    Rect                    GetContentsRect() const { auto x0 = x+GetPinSize()+GetInputColumnLabelWidth()+GetColumnPadding(); return {x0, y, x0+GetContentsColumnWidth(), y+GetSizeY()}; }
};
std::vector<Node> nodes;

void OnIdle() 
{ 
    glutPostRedisplay(); 
}

void RenderText(int x, int y, const std::string & text)
{
    glRasterPos2i(x,y+18);
    for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, ch);
}

void OnDisplay()
{  
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glOrtho(0, 1280, 720, 0, -1, 1);

    // Draw node contents
    for(const auto & n : nodes)
    {
        auto rect = n.GetNodeRect();
        glBegin(GL_QUADS);
        glColor3f(0.3f,0.3f,0.3f);
        glVertex2i(rect.x0,rect.y0);
        glVertex2i(rect.x1,rect.y0);
        glVertex2i(rect.x1,rect.y1);
        glVertex2i(rect.x0,rect.y1);
        glEnd();

        rect = n.GetContentsRect();
        if(n.function)
        {
            glColor3f(1,1,1);
            RenderText(rect.x0, rect.y0, ToString() << n.function->GetName());
            rect.y0 += n.GetPinSpacing();
        }
       
        glColor3f(0.7f,0.7f,0.7f);
        RenderText(rect.x0, rect.y0, ToStr(*n.type.type, n.value.get()));

        for(size_t i=0; i<n.GetInputCount(); ++i)
        {
            rect = n.GetInputPinRect(i);
            glBegin(GL_QUADS);
            glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0,rect.y0);
            glVertex2i(rect.x1,rect.y0);
            glVertex2i(rect.x1,rect.y1);
            glVertex2i(rect.x0,rect.y1);
            glEnd();

            glColor3f(1,1,1);
            RenderText(rect.x1 + n.GetPinPadding(), rect.y0, ToString() << n.function->GetParamTypes()[i]);
        }

        for(size_t i=0; i<n.GetOutputCount(); ++i)
        {
            rect = n.GetOutputPinRect(i);
            glBegin(GL_QUADS);
            glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0,rect.y0);
            glVertex2i(rect.x1,rect.y0);
            glVertex2i(rect.x1,rect.y1);
            glVertex2i(rect.x0,rect.y1);
            glEnd();

            std::string s = ToString() << n.type;
            glColor3f(1,1,1);
            RenderText(rect.x0 - n.GetPinPadding() - GetStringWidth(s), rect.y0, s);
        }
    }

    // Draw wires
    glBegin(GL_LINES);
    glColor3f(1,1,1);
    for(const auto & n : nodes)
    {
        for(size_t i=0; i<n.inputs.size(); ++i)
        {
            if(n.inputs[i] >= 0 && n.inputs[i] < nodes.size())
            {
                auto a = nodes[n.inputs[i]].GetOutputPinRect(0);
                auto b = n.GetInputPinRect(i);
                glVertex2f((a.x0+a.x1)*0.5f, (a.y0+a.y1)*0.5f);
                glVertex2f((b.x0+b.x1)*0.5f, (b.y0+b.y1)*0.5f);
            }
        }
    }
    glEnd();
   
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

    nodes[0] = Node(100, 100, types, 2);
    nodes[1] = Node(100, 300, types, 3);

    nodes[2] = Node(500, 200, types.GetFunction("add"));
    nodes[2].inputs = {0,1};

    nodes[3] = Node(500, 400, types, 8);

    nodes[4] = Node(900, 300, types.GetFunction("mul"));
    nodes[4].inputs = {2,3};

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