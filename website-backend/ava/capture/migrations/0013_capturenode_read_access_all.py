# -*- coding: utf-8 -*-
# Generated by Django 1.10.1 on 2016-11-21 13:08
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('capture', '0012_auto_20161110_0814'),
    ]

    operations = [
        migrations.AddField(
            model_name='capturenode',
            name='read_access_all',
            field=models.BooleanField(default=False),
        ),
    ]
