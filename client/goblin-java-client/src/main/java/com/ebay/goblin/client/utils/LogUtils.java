package com.ebay.goblin.client.utils;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public final class LogUtils {
    private static final Logger logger = LoggerFactory.getLogger(LogUtils.class);

    private static GoblinLogLevel logLevel = GoblinLogLevel.INFO;

    public static void setLogLevel(GoblinLogLevel logLevel) {
        LogUtils.logLevel = logLevel;
    }

    public static void debug(String msg) {
        if (debugEnabled()) {
            logger.debug(msg);
        }
    }

    public static void debug(String format, Object arg) {
        if (debugEnabled()) {
            logger.debug(format, arg);
        }
    }

    public static void debug(String format, Object arg1, Object arg2) {
        if (debugEnabled()) {
            logger.debug(format, arg1, arg2);
        }
    }

    public static void debug(String format, Object... arguments) {
        if (debugEnabled()) {
            logger.debug(format, arguments);
        }
    }

    public static void debug(String msg, Throwable t) {
        if (debugEnabled()) {
            logger.debug(msg, t);
        }
    }

    public static void info(String msg) {
        if (infoEnabled()) {
            logger.info(msg);
        }
    }

    public static void info(String format, Object arg) {
        if (infoEnabled()) {
            logger.info(format, arg);
        }
    }

    public static void info(String format, Object arg1, Object arg2) {
        if (infoEnabled()) {
            logger.info(format, arg1, arg2);
        }
    }

    public static void info(String format, Object... arguments) {
        if (infoEnabled()) {
            logger.info(format, arguments);
        }
    }

    public static void info(String msg, Throwable t) {
        if (infoEnabled()) {
            logger.info(msg, t);
        }
    }

    public static void warn(String msg) {
        if (warnEnabled()) {
            logger.warn(msg);
        }
    }

    public static void warn(String format, Object arg) {
        if (warnEnabled()) {
            logger.warn(format, arg);
        }
    }

    public static void warn(String format, Object... arguments) {
        if (warnEnabled()) {
            logger.warn(format, arguments);
        }
    }

    public static void warn(String format, Object arg1, Object arg2) {
        if (warnEnabled()) {
            logger.warn(format, arg1, arg2);
        }
    }

    public static void warn(String msg, Throwable t) {
        if (warnEnabled()) {
            logger.warn(msg, t);
        }
    }

    public static void error(String msg) {
        if (errorEnabled()) {
            logger.error(msg);
        }
    }

    public static void error(String format, Object arg) {
        if (errorEnabled()) {
            logger.error(format, arg);
        }
    }

    public static void error(String format, Object arg1, Object arg2) {
        if (errorEnabled()) {
            logger.error(format, arg1, arg2);
        }
    }

    public static void error(String format, Object... arguments) {
        if (errorEnabled()) {
            logger.error(format, arguments);
        }
    }

    public static void error(String msg, Throwable t) {
        if (errorEnabled()) {
            logger.error(msg, t);
        }
    }

    private static boolean debugEnabled() {
        return logLevel.getLevel() <= GoblinLogLevel.DEBUG.getLevel();
    }

    private static boolean infoEnabled() {
        return logLevel.getLevel() <= GoblinLogLevel.INFO.getLevel();
    }

    private static boolean warnEnabled() {
        return logLevel.getLevel() <= GoblinLogLevel.WARN.getLevel();
    }

    private static boolean errorEnabled() {
        return logLevel.getLevel() <= GoblinLogLevel.ERROR.getLevel();
    }
}

