package com.ebay.goblin.client.model;

import com.ebay.goblin.client.model.common.ReadEntry;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;


@Getter
@Setter
@ToString(callSuper = true)
public class DeleteRequest extends ReadEntry {

    private boolean returnValue = false;

}
