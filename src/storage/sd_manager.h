#pragma once

void initSD();
void printCardInfo();
bool wakeUpSDCard();
bool wakeUpSDCardRobust();
bool quickSDCheck();
bool wakeUpSDCardWithRetry(int maxAttempts = 3); 