#include <stdio.h>
#include <ctype.h>
#include <raylib.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui/src/raygui.h"

#include <curl/curl.h>
#include "cJSON/cJSON.h"

#define SCALE_FACTOR 1.3
#define WINDOW_WIDTH 1152*SCALE_FACTOR
#define WINDOW_HEIGHT 768*SCALE_FACTOR
#define SIDEBAR_WIDTH 0.3*WINDOW_WIDTH
#define SIDEBAR_BUTTON_HEIGHT 60
#define UI_GAP 20

#define BACKGROUND_COLOR (Color){69, 69, 69, 255}
#define SIDEBAR_COLOR (Color){45, 45, 45, 255}
#define MESSAGE_INPUT_BACKGROUND (Color){55, 55, 55, 255}
#define MESSAGE_COLOR (Color){255, 255, 255, 255}
#define ASSISTANT_MESSAGE_BACKGROUND (Color){52, 52, 52, 255}
#define SIDEBAR_BUTTON_BACKGROUND_COLOR (Color){255, 255, 255, 255}
#define SIDEBAR_BUTTON_COLOR (Color){0, 0, 0, 255}

#define MAX_CONVERSATIONS 25
#define MAX_MESSAGES 50
#define MAX_TITLE_LEN 64
#define MESSAGE_MAX_LEN 2048
#define LINE_MAX_LEN 1024
#define MESSAGE_INPUT_HEIGHT 65
#define MESSAGE_INPUT_PADDING 25
#define MESSAGE_INPUT_FONT_SIZE 30
#define MESSAGE_FONT_SIZE 30
#define MESSAGE_PADDING 40
#define SIDEBAR_BUTTON_FONT_SIZE 25
#define JSON_MAX_LEN 2048
#define SCROLL_SPEED 20

size_t write_callback( void *content, size_t size, size_t nmemb, void *userp ) {
    size_t total_size = size * nmemb;
    char json_chunk[JSON_MAX_LEN] = "\0";
    sprintf( json_chunk, "%.*s", (int)nmemb, (char *)content );
    strcat( (char *)userp, json_chunk );
    return total_size;
}

char *chatgpt_get_response( char *response_buffer, char *user_message ) {
    CURL *curl = curl_easy_init();

    if( curl == NULL ) {
        fprintf( stderr, "ERROR: Unable to intialize Curl\n" );
        return NULL;
    }

    char *api_key = getenv( "OPENAI_API_KEY" );
    if( api_key == NULL ) {
        fprintf( stderr, "NOTICE: No OpenAI API key provided\n" );
        char *api_key = "";
    }

    char authorization_header[128] = "Authorization: Bearer ";
    strcat( authorization_header, api_key );

    struct curl_slist *headers = NULL;
    headers = curl_slist_append( headers, "Content-Type: application/json" );
    headers = curl_slist_append( headers, authorization_header );

    char json_payload[JSON_MAX_LEN];
    sprintf( json_payload, "{\"model\": \"gpt-3.5-turbo\", \"messages\": [{\"role\": \"user\", \"content\": \"%s\"}]}", user_message );

    curl_easy_setopt( curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions" );
    curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );
    curl_easy_setopt( curl, CURLOPT_POST, 1 );
    curl_easy_setopt( curl, CURLOPT_POSTFIELDS, json_payload );
    curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_callback );
    curl_easy_setopt( curl, CURLOPT_WRITEDATA, response_buffer );

    CURLcode res = curl_easy_perform( curl );

    if( res != CURLE_OK ) {
        fprintf( stderr, "ERROR: Unable to perform request: %s\n", curl_easy_strerror( res ) );
        return NULL;
    }

    curl_slist_free_all( headers );
    curl_easy_cleanup( curl );

    return response_buffer;
}

char *chatgpt_parse_json( char *content_buffer, char *json ) {
    cJSON *root = cJSON_Parse( json );

    if( root == NULL ) {
        fprintf( stderr, "ERROR: Unable to parse JSON: %s\n", cJSON_GetErrorPtr() );
        return NULL;
    }

    cJSON *choices = cJSON_GetObjectItem( root, "choices" );

    if( choices == NULL ) {
        fprintf( stderr, "ERROR: Unable to get choices: %s\n", cJSON_GetErrorPtr() );
        return NULL;
    }

    if( ! cJSON_IsArray( choices ) ) {
        fprintf( stderr, "ERROR: choices is not an array\n" );
        return NULL;
    }

    cJSON *choice = cJSON_GetArrayItem( choices, 0 );

    if( choice == NULL ) {
        fprintf( stderr, "ERROR: Unable to get choice: %s\n", cJSON_GetErrorPtr() );
        return NULL;
    }

    if( ! cJSON_IsObject( choice ) ) {
        fprintf( stderr, "ERROR: choice is not an object\n" );
        return NULL;
    }

    cJSON *message = cJSON_GetObjectItem( choice, "message" );

    if( message == NULL ) {
        fprintf( stderr, "ERROR: Unable to get message: %s\n", cJSON_GetErrorPtr() );
        return NULL;
    }

    if( ! cJSON_IsObject( message ) ) {
        fprintf( stderr, "ERROR: message is not an object\n" );
        return NULL;
    }

    cJSON *content = cJSON_GetObjectItem( message, "content" );

    if( content == NULL ) {
        fprintf( stderr, "ERROR: Unable to get content: %s\n", cJSON_GetErrorPtr() );
        return NULL;
    }

    if( ! cJSON_IsString( content ) ) {
        fprintf( stderr, "ERROR: content is not a string\n" );
        return NULL;
    }

    strcpy( content_buffer, content->valuestring );

    return content_buffer;
}

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
}

Vector2 draw_text_wrapped( int width, Font font, const char *text, Vector2 position, float fontSize, float spacing, Color text_color, Color background_color ) {
    int text_length = strlen( text );

    char line_buffer[LINE_MAX_LEN] = "\0";
    int line_index = 0;

    int message_height = MESSAGE_PADDING;
    int text_space_pos = -1;
    int line_space_pos = -1;

    position.y += MESSAGE_PADDING;
    bool is_first_line = true;

    for( int text_index = 0; text_index < text_length; text_index++, line_index++ ) {
        if( line_index > LINE_MAX_LEN - 2 ) {
            fprintf( stderr, "ERROR: Line too long" );
            break;
        }

        if( isspace( text[text_index] ) ) {
            text_space_pos = text_index;
            line_space_pos = line_index;
        }

        line_buffer[line_index] = text[text_index];
        line_buffer[line_index + 1] = '\0';

        bool is_newline = line_buffer[line_index] == '\n';
        bool is_last_character = text_index == text_length - 1;
        bool is_first_character = line_index == 0;

        if( is_newline && ! is_last_character ) {
            line_buffer[line_index] = '\0';
        }

        Vector2 text_size = MeasureTextEx( font, line_buffer, fontSize, spacing );
        int line_width = text_size.x;
        int line_height = text_size.y;

        if( line_width > width || is_newline || is_last_character ) {
            if( ! is_last_character && ! is_first_character )  {
                if( line_space_pos > -1 && ! is_newline ) {
                    line_buffer[line_space_pos] = '\0';
                } else {
                    line_buffer[line_index] = '\0';
                }
            }

            int top_padding = 0;
            if( is_first_line ) {
                top_padding = MESSAGE_PADDING;
                is_first_line = false;
            }

            DrawRectangle(
                position.x - UI_GAP,
                position.y - top_padding,
                width + UI_GAP * 2,
                line_height + MESSAGE_PADDING + top_padding,
                background_color
            );

            DrawTextEx( font, line_buffer, position, fontSize, spacing, text_color );

            position.y += line_height;
            message_height += line_height + top_padding;

            if( ! is_newline && ! is_last_character && ! is_first_character ) {
                if( line_space_pos > -1 ) {
                    text_index = text_space_pos;
                } else {
                    text_index--;
                }
            }

            line_index = -1;
            line_space_pos = -1;
        }
    }

    return (Vector2){width, message_height};
}

void message_list_draw( char message_list[MAX_MESSAGES][MESSAGE_MAX_LEN], int message_count, Font font ) {
    static int current_scroll = 0;
    static float scroll = 0;

    scroll += GetMouseWheelMove() * SCROLL_SPEED;
    scroll *= 0.9;

    current_scroll += scroll;

    if( current_scroll > 0 ) {
        current_scroll = 0;
    }

    int x = SIDEBAR_WIDTH + UI_GAP;
    int y = current_scroll;
    int width = GetRenderWidth() - SIDEBAR_WIDTH - UI_GAP * 2;
    int height = GetRenderHeight() - UI_GAP * 2 - MESSAGE_INPUT_HEIGHT;

    BeginScissorMode(
        SIDEBAR_WIDTH,
        0,
        width + UI_GAP * 2,
        height
    );

    int message_list_height = 0;

    for( int i = 0; i < message_count; i++ ) {
        char *message = message_list[i];
        Vector2 position = (Vector2){x, y};

        Color message_background = BACKGROUND_COLOR;
        if( i % 2 != 0 ) {
            message_background = ASSISTANT_MESSAGE_BACKGROUND;
        }

        Vector2 text_size = draw_text_wrapped( width, font, message, position, MESSAGE_FONT_SIZE, 0, MESSAGE_COLOR, message_background );

        int message_height = text_size.y;
        message_list_height += message_height;
        y += message_height;
    }

    int max_scroll = -message_list_height + height;

    if( current_scroll < max_scroll ) {
        current_scroll = max_scroll;
    }

    EndScissorMode();
}

void conversation_list_draw( char message_list[MAX_CONVERSATIONS][MAX_MESSAGES][MESSAGE_MAX_LEN], int *conversation_count, int *current_conversation, Font font ) {
    int x = UI_GAP;
    int y = UI_GAP;
    int width = SIDEBAR_WIDTH - UI_GAP * 2;
    int height = SIDEBAR_BUTTON_HEIGHT;

    Rectangle rec = (Rectangle){ x, y, width, height };
    DrawRectangleRounded( rec, 0.5f, 1, SIDEBAR_BUTTON_BACKGROUND_COLOR );

    char button_text[MAX_TITLE_LEN] = "+ New chat";

    Vector2 text_size = MeasureTextEx( font, button_text, SIDEBAR_BUTTON_FONT_SIZE, 0 );
    int text_width = text_size.x;
    int text_height = text_size.y;
    int text_x = x + width / 2 - text_width / 2;
    int text_y = y + height / 2 - text_height / 2;

    DrawTextEx( font, button_text, (Vector2){text_x, text_y}, SIDEBAR_BUTTON_FONT_SIZE, 0, SIDEBAR_BUTTON_COLOR );

    if( IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) && CheckCollisionPointRec( GetMousePosition(), rec ) ) {
        (*current_conversation)++;
        (*conversation_count)++;
    }

    y += rec.height + UI_GAP;

    for( int i = 0; i < *conversation_count; i++ ) {
        Rectangle rec = (Rectangle){ x, y, width, height };
        DrawRectangleRounded( rec, 0.5f, 1, SIDEBAR_BUTTON_BACKGROUND_COLOR );

        char button_text[MAX_TITLE_LEN];
        sprintf( button_text, "Conversation %d", i + 1 );

        Vector2 text_size = MeasureTextEx( font, button_text, SIDEBAR_BUTTON_FONT_SIZE, 0 );
        int text_width = text_size.x;
        int text_height = text_size.y;
        int text_x = x + width / 2 - text_width / 2;
        int text_y = y + height / 2 - text_height / 2;

        DrawTextEx( font, button_text, (Vector2){text_x, text_y}, SIDEBAR_BUTTON_FONT_SIZE, 0, SIDEBAR_BUTTON_COLOR );

        if( IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) && CheckCollisionPointRec( GetMousePosition(), rec ) ) {
            *current_conversation = i;
        }

        y += rec.height + UI_GAP;
    }
}

int main() {
    SetConfigFlags( FLAG_WINDOW_RESIZABLE );
    InitWindow( WINDOW_WIDTH, WINDOW_HEIGHT, "ChatGPT-C" );
    SetWindowMinSize( 800, 600 );

    SetTargetFPS( 60 );

    // styles for message input
    GuiSetStyle( DEFAULT, BORDER_WIDTH, 0 );
    GuiSetStyle( DEFAULT, BASE_COLOR_PRESSED, 0x00000000 );
    GuiSetStyle( DEFAULT, TEXT_COLOR_PRESSED, 0xffffffff );
    GuiSetStyle( DEFAULT, TEXT_SIZE, MESSAGE_INPUT_FONT_SIZE );

    Font font = LoadFont( "Ubuntu-R.ttf" );
    GuiSetFont( font );

    char message_list[MAX_CONVERSATIONS][MAX_MESSAGES][MESSAGE_MAX_LEN];
    int conversation_count = 1;
    int current_conversation = 0;
    int message_count[MAX_CONVERSATIONS];

    char user_message_buffer[MESSAGE_MAX_LEN] = "\0";
    char chatgpt_message[MESSAGE_MAX_LEN] = "\0";
    char chatgpt_response[JSON_MAX_LEN] = "\0";
    char chatgpt_response_content[MESSAGE_MAX_LEN] = "\0";

#if 1
    message_list_add( message_list[current_conversation], &message_count[current_conversation], "First hello" );
    for( int i = 0; i < 50; i++ ) {
        message_list_add( message_list[current_conversation], &message_count[current_conversation], "Hello, this is a very long message that should\nword-wrap when it goes to\nthe end of the screen, what will happen?" );
    }
    message_list_add( message_list[current_conversation], &message_count[current_conversation], "Last hello" );
#endif

    while( ! WindowShouldClose() ) {
        BeginDrawing();

        ClearBackground( BACKGROUND_COLOR );

        sidebar_draw();
        conversation_list_draw( message_list, &conversation_count, &current_conversation, font );
        message_input_draw( user_message_buffer );
        message_list_draw( message_list[current_conversation], message_count[current_conversation], font );

        EndDrawing();

        if( *chatgpt_message != '\0' ) {
            if( chatgpt_get_response( chatgpt_response, chatgpt_message ) == NULL ) {
                fprintf( stderr, "ERROR: There was an error in the ChatGPT request\n" );
                return 1;
            }

            if( chatgpt_parse_json( chatgpt_response_content, chatgpt_response ) == NULL ) {
                fprintf( stderr, "ERROR: There was an error in parsing the ChatGPT JSON\n" );
                return 1;
            }

            strcpy( chatgpt_response, "\0" );
            strcpy( chatgpt_message, "\0" );

            message_list_add( message_list[current_conversation], &message_count[current_conversation], chatgpt_response_content );
        }

        if( IsKeyPressed( KEY_ENTER ) && *user_message_buffer != '\0' ) {
            message_list_add( message_list[current_conversation], &message_count[current_conversation], user_message_buffer );
            strcpy( chatgpt_message, user_message_buffer );
            strcpy( user_message_buffer, "\0" );
        }
    }

    UnloadFont( font );
    CloseWindow();

    return 0;
}