package com.ebay.goblin.client.utils;

public enum GoblinLogLevel {
    DEBUG(1),
    INFO(2),
    WARN(4),
    ERROR(8);

    private int level;

    public int getLevel() {
        return level;
    }

    GoblinLogLevel(int level) {
        this.level = level;
    }
}
