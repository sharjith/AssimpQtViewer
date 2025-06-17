#version 450 core

uniform vec4 top_color;
uniform vec4 bot_color;
uniform int gradient_style;  // 0=Vertical, 1=Horizontal, 2=TopLeftToBottomRight, 3=TopRightToBottomLeft
in vec2 v_uv;

out vec4 frag_color;

void main()
{
    if (gradient_style == 0) {
        // VERTICAL GRADIENT (top to bottom)
        frag_color = mix(bot_color, top_color, v_uv.y);
    }
    else if (gradient_style == 1) {
        // HORIZONTAL GRADIENT (left to right)
        frag_color = mix(top_color, bot_color, v_uv.x);
    }
    else if (gradient_style == 2) {
        // DIAGONAL GRADIENT (top-left to bottom-right)
        float diagonal_factor = (v_uv.x + (1.0 - v_uv.y)) * 0.5;
        frag_color = mix(top_color, bot_color, diagonal_factor);
    }
    else if (gradient_style == 3) {
        // DIAGONAL GRADIENT (top-right to bottom-left)
        float diagonal_factor = ((1.0 - v_uv.x) + (1.0 - v_uv.y)) * 0.5;
        frag_color = mix(top_color, bot_color, diagonal_factor);
    }
    else {
        // Default to vertical if invalid style
        frag_color = mix(bot_color, top_color, v_uv.y);
    }
}