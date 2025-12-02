#include <GL/freeglut.h>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <string>
#include <iostream>

using namespace std;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// -------------------- GLOBAL VARIABLES --------------------
string highscoreFile = "highscore.txt";
bool bankrupt = false;
int highestMoney = 0;

int windowWidth = 1280, windowHeight = 720;   // updated resolution

const int MAX_CARDS = 12;
const int NUM_SUITS = 4;
const char suitChars[NUM_SUITS] = { 'D', 'H', 'C', 'S' };
const char* suitNames[NUM_SUITS] = { "Diamond", "Heart", "Club", "Spade" };

GLuint cardTextures[13][4]; // [value-1][suit]
GLuint cardBackTexture;     // For hidden dealer cards
GLuint tableTexture;        // Background poker table texture

// Card size (scaled for 1280x720)
const float CARD_W = 90.0f;
const float CARD_H = 126.0f;

// -------------------- CARD CLASS --------------------
class Card {
    int value;  // 1-13
    int suit;   // 0-3
public:
    Card() { value = 0, suit = 0; }
    Card(int v, int s) { value = v, suit = s; }
    int getValue() const { return value; }
    int getSuit() const { return suit; }
};

Card drawCard() {
    int val = rand() % 13 + 1;
    int s = rand() % NUM_SUITS;
    return Card(val, s);
}

int cardValue(int val) {
    if (val > 10) return 10;
    if (val == 1) return 11; // Ace initially 11
    return val;
}

int getScore(const Card hand[], int cardCount) {
    int score = 0, aces = 0;
    for (int i = 0; i < cardCount; i++) {
        score += cardValue(hand[i].getValue());
        if (hand[i].getValue() == 1) aces++;
    }
    while (score > 21 && aces > 0) {
        score -= 10;
        aces--;
    }
    return score;
}

// -------------------- TEXT --------------------
void drawText(float x, float y, const string& text, float r = 0, float g = 0, float b = 0) {
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    for (char c : text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
}

string cardValueToStr(int val) {
    if (val == 1) return "A";
    if (val == 11) return "J";
    if (val == 12) return "Q";
    if (val == 13) return "K";
    return to_string(val);
}

char suitToChar(int s) {
    if (s < 0 || s >= NUM_SUITS) return '?';
    return suitChars[s];
}

// -------------------- TEXTURE LOADER --------------------
GLuint loadTexture(const char* filename) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* image = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);
    if (!image) {
        cerr << "Failed to load " << filename << endl;
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(image);
    return tex;
}

void loadAllCardTextures() {
    const char* suits[] = { "D", "H", "C", "S" };
    for (int v = 1; v <= 13; v++) {
        for (int s = 0; s < NUM_SUITS; s++) {
            string filename = "cards/" + cardValueToStr(v) + suits[s] + ".png";
            cardTextures[v-1][s] = loadTexture(filename.c_str());
        }
    }
    cardBackTexture = loadTexture("cards/back.png");
    tableTexture = loadTexture("table.png");  // Poker table background
}

// -------------------- DRAW HELPERS --------------------
void drawTexturedQuad(GLuint tex, float x, float y, float w, float h) {
    if (!tex) return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);

    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(x, y);
        glTexCoord2f(1, 0); glVertex2f(x + w, y);
        glTexCoord2f(1, 1); glVertex2f(x + w, y + h);
        glTexCoord2f(0, 1); glVertex2f(x, y + h);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

void drawCardShape(float x, float y, Card card, bool hidden = false) {
    if (hidden) {
        drawTexturedQuad(cardBackTexture, x, y, CARD_W, CARD_H);
    } else {
        int v = card.getValue() - 1;
        int s = card.getSuit();
        drawTexturedQuad(cardTextures[v][s], x, y, CARD_W, CARD_H);
    }
}

// -------------------- GAME STATE --------------------
Card playerHand[MAX_CARDS];
int playerCardCount = 0;
Card dealerHand[MAX_CARDS];
int dealerCardCount = 0;

int playerMoney = 1000;
int currentBet = 100;
bool betConfirmed = false;

bool playerTurn = true;
bool gameOver = false;
string gameMessage = "Set your bet: Use Up/Down arrows, Enter to deal";

// -------------------- GAME LOGIC --------------------
void updateHighScore() {
    if (playerMoney > highestMoney) {
        highestMoney = playerMoney;
        ofstream out(highscoreFile);
        out << highestMoney;
        out.close();
    }
}

void evaluateResult() {
    int playerScore = getScore(playerHand, playerCardCount);
    int dealerScore = getScore(dealerHand, dealerCardCount);

    if (dealerScore > 21 || playerScore > dealerScore) {
        playerMoney += currentBet;
        gameMessage = "You win! Press R to continue.";
    }
    else if (dealerScore == playerScore) {
        gameMessage = "Push (Tie). Press R to continue.";
    }
    else {
        playerMoney -= currentBet;
        gameMessage = "Dealer wins. Press R to continue.";
    }

    if (playerMoney <= 0) {
        playerMoney = 0;
        bankrupt = true;
        gameMessage = "Game Over! You are bankrupt. Press R to restart.";
        currentBet = 100;
    }

    gameOver = true;
    updateHighScore();
}

void playerHit() {
    if (!playerTurn || gameOver) return;
    if (playerCardCount >= MAX_CARDS) return;
    playerHand[playerCardCount++] = drawCard();

    int score = getScore(playerHand, playerCardCount);
    if (score > 21) {
        playerMoney -= currentBet;
        if (playerMoney <= 0) {
            playerMoney = 0;
            bankrupt = true;
            gameMessage = "Bust! You are bankrupt. Press R to restart.";
        }
        else {
            gameMessage = "Bust! You lose. Press R to continue.";
        }
        gameOver = true;
        updateHighScore();
    }
}

void dealerTurn() {
    while (getScore(dealerHand, dealerCardCount) < 17) {
        if (dealerCardCount >= MAX_CARDS) break;
        dealerHand[dealerCardCount++] = drawCard();
    }
    evaluateResult();
}

void playerStand() {
    if (!playerTurn || gameOver) return;
    playerTurn = false;
    dealerTurn();
}

void startGame() {
    if (betConfirmed || currentBet > playerMoney || bankrupt) return;

    playerCardCount = 0;
    dealerCardCount = 0;

    playerHand[playerCardCount++] = drawCard();
    playerHand[playerCardCount++] = drawCard();

    dealerHand[dealerCardCount++] = drawCard();
    dealerHand[dealerCardCount++] = drawCard();

    betConfirmed = true;
    playerTurn = true;
    gameOver = false;
    gameMessage = "Hit (H) or Stand (S)";
}

// -------------------- RENDER --------------------
void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw poker table background stretched to fit
    drawTexturedQuad(tableTexture, 0, 0, windowWidth, windowHeight);

    // HUD positions for 1280x720
    drawText(950, 680, "High Score: $" + to_string(highestMoney));
    drawText(50, 680, "Money: $" + to_string(playerMoney));

    if (!betConfirmed) {
        drawText(50, 650, "Current Bet: $" + to_string(currentBet));
    }

    if (betConfirmed) {
        float startX = 200, startY = 100;
        drawText(startX, startY + CARD_H + 20,
                 "Player (Score: " + to_string(getScore(playerHand, playerCardCount)) + ")");
        for (int i = 0; i < playerCardCount; i++)
            drawCardShape(startX + i * (CARD_W + 10), startY, playerHand[i]);

        startX = 200; startY = 350;
        if (!gameOver)
            drawText(startX, startY + CARD_H + 20, "Dealer (Score: ?)");
        else
            drawText(startX, startY + CARD_H + 20,
                     "Dealer (Score: " + to_string(getScore(dealerHand, dealerCardCount)) + ")");

        for (int i = 0; i < dealerCardCount; i++) {
            if (i == 1 && !gameOver) {
                drawCardShape(startX + i * (CARD_W + 10), startY, dealerHand[i], true);
            } else {
                drawCardShape(startX + i * (CARD_W + 10), startY, dealerHand[i], false);
            }
        }
    }

    drawText(50, 620, gameMessage, 0, 0, 0);

    if (bankrupt) {
        drawText(540, 360, "GAME OVER", 1, 0, 0);
    }

    glutSwapBuffers();
}

// -------------------- INPUT --------------------
void keyPress(unsigned char key, int x, int y) {
    if (key == 27) exit(0);

    if (!betConfirmed) {
        if (key == 13) {
            startGame();
            glutPostRedisplay();
            return;
        }
    }

    if (gameOver && (key == 'r' || key == 'R')) {
        if (bankrupt) {
            playerMoney = 1000;
            bankrupt = false;            
        }
        betConfirmed = false;
        gameOver = false;
        gameMessage = "Set your bet: Use Up/Down arrows, Enter to deal";
        glutPostRedisplay();
        return;
    }

    if (key == 'h' || key == 'H') {
        playerHit();
    }
    else if (key == 's' || key == 'S') {
        playerStand();
    }

    glutPostRedisplay();
}

void specialKeyPress(int key, int x, int y) {
    if (!betConfirmed) {
        if (key == GLUT_KEY_UP && currentBet + 10 <= playerMoney) {
            currentBet += 10;
        }
        else if (key == GLUT_KEY_DOWN && currentBet - 10 >= 10) {
            currentBet -= 10;
        }
        glutPostRedisplay();
    }
}

// -------------------- INIT --------------------
void initialize() {
    glClearColor(0, 0.6f, 0, 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, 0, windowHeight);
}

// -------------------- MAIN --------------------
int main(int argc, char** argv) {
    srand(time(0));
    ifstream in(highscoreFile);
    if (in >> highestMoney) {}
    else highestMoney = 1000;
    in.close();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Blackjack by Murtuja Afroz Saad (240109)");

    initialize();
    loadAllCardTextures();

    glutDisplayFunc(renderScene);
    glutKeyboardFunc(keyPress);
    glutSpecialFunc(specialKeyPress);

    glutMainLoop();
    return 0;
}
