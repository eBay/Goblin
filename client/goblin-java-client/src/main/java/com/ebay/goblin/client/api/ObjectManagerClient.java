package com.ebay.goblin.client.api;


import com.ebay.goblin.client.exceptions.GoblinException;
import com.ebay.goblin.client.model.common.RouteResult;

public interface ObjectManagerClient extends AutoCloseable {

    RouteResult route() throws GoblinException;

}
