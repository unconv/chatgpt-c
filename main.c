#include <stdio.h>
#include <raylib.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui/src/raygui.h"

#define SCALE_FACTOR 1.3
#define WINDOW_WIDTH 1152*SCALE_FACTOR
#define WINDOW_HEIGHT 768*SCALE_FACTOR
#define SIDEBAR_WIDTH 0.3*WINDOW_WIDTH
#define UI_GAP 20

#define BACKGROUND_COLOR (Color){69, 69, 69, 255}
#define SIDEBAR_COLOR (Color){45, 45, 45, 255}
#define MESSAGE_INPUT_BACKGROUND (Color){55, 55, 55, 255}
#define MESSAGE_COLOR (Color){255, 255, 255, 255}

#define MAX_MESSAGES 255
#define MESSAGE_MAX_LEN 2048
#define MESSAGE_INPUT_HEIGHT 65
#define MESSAGE_INPUT_PADDING 25
#define MESSAGE_INPUT_FONT_SIZE 30
#define MESSAGE_FONT_SIZE 30
#define MESSAGE_GAP 20

void sidebar_draw() {
    DrawRectangle( 0, 0, SIDEBAR_WIDTH, GetRenderHeight(), SIDEBAR_COLOR );
}

void message_input_draw( char *text_buffer ) {
    int x = SIDEBAR_WIDTH + UI_GAP;
    int y = GetRenderHeight() - UI_GAP - MESSAGE_INPUT_HEIGHT;
    int width = GetRenderWidth() - UI_GAP * 2 - SIDEBAR_WIDTH;

    Rectangle bg_bounds = (Rectangle){
        x,
        y,
        width,
        MESSAGE_INPUT_HEIGHT
    };

    Rectangle text_bounds = (Rectangle){
        x + MESSAGE_INPUT_PADDING / 2,
        y + MESSAGE_INPUT_PADDING / 2,
        width,
        MESSAGE_INPUT_HEIGHT-MESSAGE_INPUT_PADDING
    };

    DrawRectangleRounded( bg_bounds, 0.5f, 1, MESSAGE_INPUT_BACKGROUND );
    GuiTextBox( text_bounds, text_buffer, MESSAGE_MAX_LEN, true );
}

void message_list_add( char message_list[MAX_MESSAGES][MESSAGE_MAX_LEN], int *message_count, char *message ) {
    strncpy( message_list[(*message_count)++], message, MESSAGE_MAX_LEN );
    strcpy( message, "\0" );
}

void message_list_draw( char message_list[MAX_MESSAGES][MESSAGE_MAX_LEN], int message_count, Font font ) {
    int x = SIDEBAR_WIDTH + UI_GAP;
    int y = UI_GAP;

    for( int i = 0; i < message_count; i++ ) {
        char *message = message_list[i];
        Vector2 position = (Vector2){x, y};
        Vector2 text_size = MeasureTextEx( font, message, MESSAGE_FONT_SIZE, 0 );

        int text_height = text_size.y;

        DrawTextEx( font, message, position, MESSAGE_FONT_SIZE, 0, MESSAGE_COLOR );
        y += text_height + MESSAGE_GAP;
    }
}

int main() {
    SetConfigFlags( FLAG_WINDOW_RESIZABLE );
    InitWindow( WINDOW_WIDTH, WINDOW_HEIGHT, "ChatGPT-C" );

    SetTargetFPS( 60 );

    // styles for message input
    GuiSetStyle( DEFAULT, BORDER_WIDTH, 0 );
    GuiSetStyle( DEFAULT, BASE_COLOR_PRESSED, 0x00000000 );
    GuiSetStyle( DEFAULT, TEXT_COLOR_PRESSED, 0xffffffff );
    GuiSetStyle( DEFAULT, TEXT_SIZE, MESSAGE_INPUT_FONT_SIZE );

    Font font = LoadFont( "Ubuntu-R.ttf" );
    GuiSetFont( font );

    char message_list[MAX_MESSAGES][MESSAGE_MAX_LEN];
    int message_count = 0;

    char user_message_buffer[MESSAGE_MAX_LEN] = "\0";

    while( ! WindowShouldClose() ) {
        BeginDrawing();

        ClearBackground( BACKGROUND_COLOR );

        sidebar_draw();
        message_input_draw( user_message_buffer );
        message_list_draw( message_list, message_count, font );

        if( IsKeyPressed( KEY_ENTER ) && *user_message_buffer != '\0' ) {
            message_list_add( message_list, &message_count, user_message_buffer );
            printf("Added message %d\n", message_count);
        }

        EndDrawing();
    }

    UnloadFont( font );
    CloseWindow();

    return 0;
}