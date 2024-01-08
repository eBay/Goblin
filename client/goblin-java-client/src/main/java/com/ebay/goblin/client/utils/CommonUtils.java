package com.ebay.goblin.client.utils;

import org.apache.commons.lang3.StringUtils;

public final class CommonUtils {

    private static final String DATACENTER_ENV_KEY = "environment.instance.datacenter";

    private static final String DC_NAME;

    static {
        DC_NAME = System.getenv(DATACENTER_ENV_KEY);
    }

    public static String getDataCenterName() {
        return StringUtils.isBlank(DC_NAME) ? "unknown" : DC_NAME.toLowerCase();
    }
}
