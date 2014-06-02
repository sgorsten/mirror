#include "graph.h"

#include <GL/glut.h>

struct Renderer
{
    void DrawText12(int x, int y, const std::string & text)
    {
        glRasterPos2i(x,y+12);
        for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, ch);
    }

    void DrawText18(int x, int y, const std::string & text)
    {
        glRasterPos2i(x,y+18);
        for(auto ch : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, ch);
    }

    void DrawWireBox(int x0, int y0, int x1, int y1)
    {
        glBegin(GL_LINE_STRIP);
        glVertex2i(x0,y0);
        glVertex2i(x1,y0);
        glVertex2i(x1,y1);
        glVertex2i(x0,y1);
        glVertex2i(x0,y0);
        glEnd();
    }

    void DrawRect(int x0, int y0, int x1, int y1)
    {
        glBegin(GL_QUADS);
        glVertex2i(x0,y0);
        glVertex2i(x1,y0);
        glVertex2i(x1,y1);
        glVertex2i(x0,y1);
        glEnd();
    }

    void DrawRect(const Rect & r)
    {
        DrawRect(r.x0, r.y0, r.x1, r.y1);
    }

    void DrawLine(const int2 & a, const int2 & b)
    {
        auto p0 = float2(a.x, a.y);
        auto p3 = float2(b.x, b.y);
        auto p1 = float2((p0.x+p3.x)/2, p0.y);
        auto p2 = float2((p0.x+p3.x)/2, p3.y);
        auto d01 = p1-p0, d12 = p2-p1, d23 = p3-p2;

        glBegin(GL_TRIANGLE_STRIP);
        for(float i=0; i<=32; ++i)
        {
            float t = i/32, s = (1-t);
            auto p = p0*(s*s*s) + p1*(3*s*s*t) + p2*(3*s*t*t) + p3*(t*t*t);
            auto d = norm(d01*(3*s*s) + d12*(6*s*t) + d23*(3*t*t));
            glVertex2f(p.x-d.y, p.y+d.x);
            glVertex2f(p.x+d.y, p.y-d.x);
        }
        glEnd();
    }
};

void GraphEditor::Draw() const
{
    Renderer r;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glOrtho(0, 1280, 720, 0, -1, 1);

    // Draw outlines for selected nodes
    glColor3f(1,1,0);
    for(const auto & n : nodes)
    {
        if(!n.selected) continue;
        auto rect = n.GetNodeRect();
        r.DrawRect(rect.x0-4, rect.y0-4, rect.x1+4, rect.y1+4);
    }

    // Draw node backgrounds
    glColor3f(0.3f,0.3f,0.3f);
    for(const auto & n : nodes)
    {
        r.DrawRect(n.GetNodeRect());
    }

    // Draw wires
    glColor3f(1,1,1);
    for(const auto & n : nodes)
    {
        if(n.IsSequenced() && n.flowOutputIndex >= 0)
        {
            r.DrawLine(n.GetFlowOutputRect().GetCenter(), nodes[n.flowOutputIndex].GetFlowInputRect().GetCenter());
        }
        for(size_t i=0; i<n.inputs.size(); ++i)
        {
            auto wire = n.inputs[i];
            if(wire.nodeIndex >= 0 && wire.nodeIndex < nodes.size())
            {
                r.DrawLine(nodes[wire.nodeIndex].GetOutputPinRect(wire.pinIndex).GetCenter(), n.GetInputPinRect(i).GetCenter());
            }
        }
    }
    if(clickedFeature.IsDataPin() || clickedFeature.IsFlowPin())
    {
        if(IsCompatible(clickedFeature, mouseOverFeature)) glColor3f(1,1,0);
        else glColor3f(0.5f,0.5f,0.5f);
        r.DrawLine(clickedFeature.GetPinRect().GetCenter(), lastPos);
    }

    // Draw node contents
    for(const auto & n : nodes)
    {
        auto rect = n.GetContentsRect();
        const auto & label = n.nodeType->GetLabel();
        if(!label.empty())
        {
            glColor3f(1,1,1);
            r.DrawText18(rect.x0, rect.y0, label);
            rect.y0 += n.GetLineSpacing();
        }

        if(n.IsSequenced())
        {
            rect = n.GetFlowInputRect();
            int cx = (rect.x0 + rect.x1) / 2;
            int cy = (rect.y0 + rect.y1) / 2;
            glBegin(GL_TRIANGLE_FAN);
            if(clickedFeature.type == Feature::FlowOutput) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0, rect.y0);
            glVertex2i(cx, rect.y0);
            glVertex2i(rect.x1, cy);
            glVertex2i(cx, rect.y1);
            glVertex2i(rect.x0, rect.y1);
            glEnd();            

            rect = n.GetFlowOutputRect();
            cx = (rect.x0 + rect.x1) / 2;
            cy = (rect.y0 + rect.y1) / 2;
            glBegin(GL_TRIANGLE_FAN);
            if(clickedFeature.type == Feature::FlowInput) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            glVertex2i(rect.x0, rect.y0);
            glVertex2i(cx, rect.y0);
            glVertex2i(rect.x1, cy);
            glVertex2i(cx, rect.y1);
            glVertex2i(rect.x0, rect.y1);
            glEnd();            
        }

        for(size_t i=0; i<n.GetInputCount(); ++i)
        {
            rect = n.GetInputPinRect(i);
            if(clickedFeature.type == Feature::Output && IsAssignable(clickedFeature.GetPinType(), n.GetInputType(i))) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            r.DrawRect(rect);

            glColor3f(1,1,1);
            r.DrawText12(rect.x1 + n.GetPinPadding(), rect.y0, n.GetInputLabel(i));
            if(n.inputs[i].nodeIndex >= 0)
            {
                auto & n2 = nodes[n.inputs[i].nodeIndex];
                auto pin = n.inputs[i].pinIndex;
                std::string val = ToStr(*n2.GetOutputType(pin).type, n2.outputValues[pin].get());
                r.DrawText12(rect.x0 - n.GetPinPadding() - GetStringWidth12(val), rect.y0, val);
            }
            else if(!n.inputs[i].immediate.empty())
            {
                std::string val = n.inputs[i].immediate;
                glColor3f(0,1,1);
                r.DrawText12(rect.x0 - n.GetPinPadding() - GetStringWidth12(val), rect.y0, val);
            }
        }

        for(size_t i=0; i<n.GetOutputCount(); ++i)
        {
            rect = n.GetOutputPinRect(i);
            if(clickedFeature.type == Feature::Input && IsAssignable(n.GetOutputType(i), clickedFeature.GetPinType())) glColor3f(1,1,0);
            else glColor3f(0.5f,0.5f,0.5f);
            r.DrawRect(rect);

            glColor3f(1,1,1);
            auto lbl = n.GetOutputLabel(i);
            r.DrawText12(rect.x0 - n.GetPinPadding() - GetStringWidth12(lbl), rect.y0, lbl);
            r.DrawText12(rect.x1 + n.GetPinPadding(), rect.y0, ToStr(*n.GetOutputType(i).type, n.outputValues[i].get()));
        }
    }

    std::string mouseOverText;
    if(mouseOverFeature.IsDataPin())
    {
        std::string mouseOverText = ToString() << mouseOverFeature.GetPinType();

        auto w = GetStringWidth12(mouseOverText);
        glColor3f(0,0,0);
        r.DrawRect(lastPos.x, lastPos.y-16, lastPos.x+w, lastPos.y);
        glEnd();

        glColor3f(1,1,1);
        r.DrawText12(lastPos.x, lastPos.y-16, mouseOverText);
    }

    switch(mode)
    {
    case Dragging:
        switch(clickedFeature.type)
        {
        case Feature::None: // Box selection
            glColor3f(0.5f,0.5f,0);
            r.DrawWireBox(clickedPos.x, clickedPos.y, lastPos.x, lastPos.y);
            glEnd();
            break;
        }
        break;
    case NewNodePopup:
        {
            int cursor = menuPos.y;
            for(const auto & type : nodeTypes)
            {
                glColor3f(1,1,1);
                r.DrawText12(menuPos.x, cursor, type.GetLabel());
                cursor += 16;
            }
        }
        break;
    }

    glPopMatrix();   
    glPopAttrib();
}